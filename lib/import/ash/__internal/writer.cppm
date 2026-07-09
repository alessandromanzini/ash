module;

#include <ash/pch.hpp>

#include <ash/cbridge.hpp>

export module ash:writer;

import :log_config;
import :signal;
import :policy;
import :format;


export namespace ash
{
   struct WriteOptions
   {
      std::optional<uint32_t> echo;
      std::source_location where;
   };

   /**
    * Representation of a string chunk with extraction metadata.
    */
   struct alignas( 64 ) Message
   {
      enum class ChunkPosition : uint8_t { sole, head, body, tail };

      using chunk_id_type = uint16_t;
      using chunk_size_type = uint16_t;
      using sequence_id_type = uint32_t;

      std::time_t timestamp;
      WriteOptions options;
      Signal signal;
      ChunkPosition chunk_position;
      chunk_id_type chunk_id;
      sequence_id_type sequence_id;
      chunk_size_type chunk_len;
      char content[222];
   };
   static_assert( alignof( Message ) == 64 && sizeof( Message ) % 64 == 0 );
}

export namespace ash
{
   /**
    * @brief Emits chunked log records to the routed sink.
    *
    * It routes a signal to the correct stream and replays a \p Message: the title line is written only on the boundary chunks, and the (already
    * formatted) content is written raw. Both the title and the content are flushed with bulk \c sputn calls — nothing is streamed to the sink
    * character-by-character unless necessary.
    */
   class Writer final
   {
      enum class Header : uint8_t { skip, write };
      enum class Trailer : uint8_t { skip, write };

   public:
      explicit Writer( cfg::WriteConfig config ) noexcept
         : config_{ std::move( config ) }
      { }

      CBR_FORCE_INLINE auto consume( Message const& message ) const noexcept -> void
      {
         switch ( message.chunk_position )
         {
            case Message::ChunkPosition::sole: emit( message, Header::write, Trailer::write ); return;
            case Message::ChunkPosition::head: emit( message, Header::write, Trailer::skip ); return;
            case Message::ChunkPosition::body: emit( message, Header::skip, Trailer::skip ); return;
            case Message::ChunkPosition::tail: emit( message, Header::skip, Trailer::write ); return;
            default                          : std::unreachable( );
         }
      }

   private:
      cfg::WriteConfig const config_;

      /**
       * Route log stream and err stream depending on the input signal.
       * LOG: trace -> warning
       * ERR: error & fatal
       * @param sig
       */
      [[nodiscard]] CBR_FORCE_INLINE auto select_sink( Signal sig ) const noexcept -> std::ostream&
      {
         return sig >= Signal::error ? config_.err_stream : config_.log_stream;
      }

      CBR_FORCE_INLINE auto emit( Message const& message, Header header, Trailer trailer ) const noexcept -> void
      {
         // Get the stream's buffer directly so we can bulk-write to it without any intermediate std::string.
         std::streambuf* const buf = select_sink( message.signal ).rdbuf( );
         //
         if ( header == Header::write )
         {
            write_title( buf, message );
         }
         //
         buf->sputn( message.content, message.chunk_len > 0 ? message.chunk_len - 1 : 0 );
         //
         if ( trailer == Trailer::write )
         {
            buf->sputn( config_.trailer_content.c_str( ), static_cast<std::streamsize>( config_.trailer_content.size( ) ) );
         }
      }

      /**
       * Format the title into a stack buffer and flush it with a single bulk \c sputn call. Only a pathologically long string falls back to a
       * per-character streamed write, so the title is never truncated.
       */
      CBR_FORCE_INLINE auto write_title( std::streambuf* buf, Message const& message ) const noexcept -> void
      {
         constexpr size_t title_capacity = 256;
         std::array<char, title_capacity> scratch;
         //
         constexpr auto fmt = "[{:u}] [{}] at {}:{}:{}\n";
         std::source_location const& loc = message.options.where;
         char const* const file_name = loc.file_name( );
         uint32_t const line = loc.line( );
         uint32_t const column = loc.column( );
         //
         auto const [out, size] =
           std::format_to_n( scratch.data( ), scratch.size( ), fmt, message.signal, config_.identity, file_name, line, column );
         //
         // If formatting fitted the scratch buffer, bulk write to stream, ...
         if ( std::cmp_less_equal( size, scratch.size( ) ) )
         {
            buf->sputn( scratch.data( ), out - scratch.data( ) );
         }
         else
         {
            // ... otherwise format character per character to stream iterator.
            std::format_to( std::ostreambuf_iterator{ buf }, fmt, message.signal, config_.identity, file_name, line, column );
         }
      }
   };

   /**
    * Direct dispatch: consume the entries on the caller thread.
    */
   class DirectDispatcher final
   {
   public:
      explicit DirectDispatcher( cfg::WriteConfig config ) noexcept
         : writer_{ std::move( config ) }
      { }

      CBR_FORCE_INLINE auto dispatch( Message const& message ) const noexcept -> void { writer_.consume( message ); }

   private:
      Writer const writer_;
   };

   /**
    * Deferred dispatch: chunks are queued in a ring buffer and drained on a dedicated worker thread.
    * The ring is a lock-free SPSC queue: the producer owns the "tail", the consumer owns the "head", and the payload is published/observed through
    * release/acquire ordering. Blocking is done with an atomic doorbell ("wait"/"notify").
    *
    * @note SPSC CONTRACT: there must be at most one producer at a time.
    *
    * @tparam pool_size The maximum queue capacity. When the queue is full, it discards the newest entry (wine policy).
    */
   template <size_t pool_size> class DeferredDispatcher final
   {
   public:
      explicit DeferredDispatcher( cfg::WriteConfig config ) noexcept
         : config_{ std::move( config ) }
      { }

      ~DeferredDispatcher( ) noexcept
      {
         // Request stop so the worker can break out of the loop and ring the doorbell to resume worker execution. Join explicitly so the ring is
         // torn down only after the worker has drained.
         // TODO: use stop_token when non-experimental in clang libc++. (last checked on 22.1.8)
         stop_token_.store( true, std::memory_order_release );
         ring_doorbell( );
         worker_.join( );
      }

      DeferredDispatcher( DeferredDispatcher const& ) noexcept = delete;
      DeferredDispatcher( DeferredDispatcher&& ) noexcept = delete;
      auto operator=( DeferredDispatcher const& ) noexcept -> DeferredDispatcher& = delete;
      auto operator=( DeferredDispatcher&& ) noexcept -> DeferredDispatcher& = delete;

      auto dispatch( Message const& entry ) noexcept -> void
      {
         size_t const tail = tail_.load( std::memory_order_relaxed );
         //
         // 1. Drop the newest entry if the ring is full.
         if ( tail - head_.load( std::memory_order_acquire ) == pool_size )
         {
            return;
         }
         //
         // 2. Write the slot, then publish it with a release store on tail_.
         ring_[tail % pool_size] = entry;
         tail_.store( tail + 1, std::memory_order_release );
         //
         // 3. Ring the doorbell so the worker wakes if it was sleeping.
         ring_doorbell( );
      }

   private:
      cfg::WriteConfig const config_;

      std::array<Message, pool_size> ring_{};
      std::atomic<size_t> head_{ 0 };       // Consumer-owned read index (monotonic) -> read by the producer for fullness
      std::atomic<size_t> tail_{ 0 };       // Producer-owned write index (monotonic) -> read by the consumer for availability
      std::atomic<uint32_t> doorbell_{ 0 }; // Bumped + notified on publish and on stop
      std::atomic<bool> stop_token_{ false };

      std::thread worker_{ [this] { run( ); } };

      auto run( ) noexcept -> void
      {
         Writer const writer{ config_ };
         //
         while ( true )
         {
            // 1. Read the doorbell BEFORE draining. This way we snapshot at which doorbell value we are before consuming the queue data.
            uint32_t const ticket = doorbell_.load( std::memory_order_acquire );
            //
            // 2. Process queue head to tail.
            size_t head = head_.load( std::memory_order_relaxed );
            size_t const tail = tail_.load( std::memory_order_acquire );
            while ( head != tail )
            {
               writer.consume( ring_[head % pool_size] );
               ++head;
            }
            //
            // 3. Free the drained slots for the producer.
            head_.store( head, std::memory_order_release );
            //
            // 4. Break the loop if stop was requested (can only happen from the destructor) and pool is drained, otherwise hold until the next
            // doorbell ring.
            if ( bool const pool_drained = head == tail_.load( std::memory_order_acquire ); is_stop_requested( ) && pool_drained )
            {
               break;
            }
            doorbell_.wait( ticket, std::memory_order_acquire );
         }
      }

      auto ring_doorbell( ) noexcept -> void
      {
         doorbell_.fetch_add( 1, std::memory_order_release );
         doorbell_.notify_one( );
      }

      auto is_stop_requested( ) const noexcept -> bool { return stop_token_.load( std::memory_order_acquire ); }
   };
}

export namespace ash
{
   template <traits::dispatch_policy P>
   using DispatchEngine =
     std::conditional_t<P::dispatch_timing == policy::DispatchTiming::deferred, DeferredDispatcher<P::pool_size>, DirectDispatcher>;
}
