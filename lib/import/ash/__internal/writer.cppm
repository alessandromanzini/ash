module;

#include <ash/pch.hpp>

#include <ash/cbridge.hpp>

export module ash:writer;

import :log_config;
import :signal;
import :policy;


export namespace ash
{
   struct WriteOptions
   {
      std::optional<uint32_t> echo;
      std::source_location where;
   };

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

namespace ash::detail
{
   class SequentialChunker final
   {
      class Iterator final
      {
      public:
         using iterator_category = std::output_iterator_tag;
         using value_type = void;
         using difference_type = std::ptrdiff_t;
         using pointer = void;
         using reference = void;

         CBR_FORCE_INLINE explicit Iterator( SequentialChunker& chunker, Message& message )
            : chunker_ptr_{ &chunker }
            , message_ptr_{ &message }
         { }

         CBR_FORCE_INLINE auto operator*( ) noexcept -> Iterator& { return *this; }
         CBR_FORCE_INLINE auto operator++( ) noexcept -> Iterator& { return *this; }
         CBR_FORCE_INLINE auto operator++( int ) const noexcept -> Iterator { return *this; }

         CBR_FORCE_INLINE auto operator=( char const c ) noexcept -> Iterator&
         {
            Message::chunk_size_type& buffer_pos = message_ptr_->chunk_len;
            if ( constexpr size_t max_msg_size = sizeof( Message::content ) - 1ULL; buffer_pos == max_msg_size )
            {
               flush( cfg::FlushAction::carry_on );
            }
            message_ptr_->content[buffer_pos++] = c;
            //
            return *this;
         }

         CBR_FORCE_INLINE auto flush( cfg::FlushAction action ) const noexcept -> void
         {
            message_ptr_->chunk_position = [this, action] -> Message::ChunkPosition {
               bool const is_first = message_ptr_->chunk_id == 0;
               bool const is_last = action == cfg::FlushAction::end;
               if ( is_first && is_last )
               {
                  return Message::ChunkPosition::sole;
               }
               if ( is_first )
               {
                  return Message::ChunkPosition::head;
               }
               if ( is_last )
               {
                  return Message::ChunkPosition::tail;
               }
               return Message::ChunkPosition::body;
            }( );
            //
            message_ptr_->chunk_id++;
            message_ptr_->content[message_ptr_->chunk_len++] = '\0';
            //
            std::invoke( chunker_ptr_->flush_fn_, *message_ptr_ );
            message_ptr_->chunk_len = 0;
         }

      private:
         SequentialChunker* chunker_ptr_ = nullptr;
         Message* message_ptr_ = nullptr;
      };

   public:
      using flush_fn_type = std::function<void( Message const& )>; // This should be std::move_only_function when clang supports it :)

      CBR_FORCE_INLINE explicit SequentialChunker(
        Signal sig, WriteOptions const& options, Message::sequence_id_type seq_id, flush_fn_type flush_fn ) noexcept
         : flush_fn_{ std::move( flush_fn ) }
         , current_chunk_{ .timestamp = std::time( nullptr ),
                           .options = options,
                           .signal = sig,
                           .chunk_position = Message::ChunkPosition::head,
                           .chunk_id = 0,
                           .sequence_id = seq_id,
                           .chunk_len = 0,
                           .content = {} }
      { }

      CBR_FORCE_INLINE auto iterator( ) noexcept -> Iterator { return Iterator{ *this, current_chunk_ }; }

   private:
      flush_fn_type const flush_fn_{};
      Message current_chunk_{};
   };
}

export namespace ash
{
   class LogWriter final
   {
      enum class Header : uint8_t { skip, write };
      enum class Trailer : uint8_t { skip, write };

   public:
      explicit LogWriter( cfg::LogConfig config ) noexcept
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
      cfg::LogConfig const config_;

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
         // Get the stream's buffer directly so we can format directly to it without any intermediate std::string.
         std::streambuf* const buf = select_sink( message.signal ).rdbuf( );
         //
         if ( header == Header::write )
         {
            std::source_location const& loc = message.options.where;
            constexpr auto fmt = "[{:u}] [{}] at {}:{}:{}\n";
            std::format_to( std::ostreambuf_iterator{ buf }, fmt, message.signal, config_.identity, loc.file_name( ), loc.line( ), loc.column( ) );
         }
         //
         buf->sputn( message.content, message.chunk_len > 0 ? message.chunk_len - 1 : 0 );
         //
         if ( trailer == Trailer::write )
         {
            buf->sputn( "\n\n", 2 );
         }
      }
   };

   /**
    * Direct dispatch: consume the entries on the caller thread.
    */
   class DirectDispatcher final
   {
   public:
      explicit DirectDispatcher( cfg::LogConfig config ) noexcept
         : writer_{ std::move( config ) }
      { }

      CBR_FORCE_INLINE auto dispatch( Message const& message ) noexcept -> void { writer_.consume( message ); }

   private:
      LogWriter writer_;
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
      explicit DeferredDispatcher( cfg::LogConfig config ) noexcept
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
      cfg::LogConfig const config_;

      std::array<Message, pool_size> ring_{};
      std::atomic<size_t> head_{ 0 };       // Consumer-owned read index (monotonic) -> read by the producer for fullness
      std::atomic<size_t> tail_{ 0 };       // Producer-owned write index (monotonic) -> read by the consumer for availability
      std::atomic<uint32_t> doorbell_{ 0 }; // Bumped + notified on publish and on stop
      std::atomic<bool> stop_token_{ false }; // Set by the destructor before the final doorbell ring

      std::thread worker_{ [this] { run( ); } };

      auto run( ) noexcept -> void
      {
         LogWriter writer{ config_ };
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

      auto is_stop_requested( ) const noexcept -> bool
      {
         return stop_token_.load( std::memory_order_acquire );
      }
   };
}

export namespace ash
{
   template <traits::dispatch_policy P>
   using DispatchEngine =
     std::conditional_t<P::dispatch_timing == policy::DispatchTiming::deferred, DeferredDispatcher<P::pool_size>, DirectDispatcher>;
}
