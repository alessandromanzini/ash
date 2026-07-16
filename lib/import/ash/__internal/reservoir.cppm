module;

#include <ash/pch.hpp>

#include <ash/cbridge.hpp>

export module ash:reservoir;


namespace ash::detail
{
   template <typename Reservoir> class ReservoirOutputIterator final
   {
   public:
      using value_type = void;
      using difference_type = ptrdiff_t;
      using pointer = void;
      using reference = void;
      using iterator_category = std::output_iterator_tag;

      explicit ReservoirOutputIterator( Reservoir& reservoir ) noexcept
         : reservoir_ptr_{ &reservoir }
      { }

      CBR_FORCE_INLINE auto operator*( ) noexcept -> ReservoirOutputIterator& { return *this; }
      CBR_FORCE_INLINE auto operator++( ) noexcept -> ReservoirOutputIterator& { return *this; }
      CBR_FORCE_INLINE auto operator++( int ) const noexcept -> ReservoirOutputIterator { return *this; }
      CBR_FORCE_INLINE auto operator=( char c ) noexcept -> ReservoirOutputIterator&
      {
         reservoir_ptr_->pour( c );
         return *this;
      }

   private:
      Reservoir* reservoir_ptr_;
   };
}

export namespace ash
{
   /**
    * The \p CharReservoir is utility container that uses a scratch buffer to cache \c char sequences in order to later bulk write to streams. When
    * its internal buffer is completely filled up (or when an explicit drain call is invoked), the sequence is dumped in the \c sink stream.
    *
    * @note If no \c sink stream is provided, the class become bounded and flags \c overflowed() once the scratch buffer is exhausted.
    *
    * @tparam CharT Character type for the internal buffer and output stream.
    * @tparam N Size of the internal scratch buffer.
    */
   template <typename CharT, size_t N> class CharReservoir final
   {
   public:
      static_assert( N > 0, "The CharReservoir's buffer must be assigned a size." );

      using value_type = std::decay_t<CharT>;
      using iterator_type = detail::ReservoirOutputIterator<CharReservoir<value_type, N>>;

      CBR_FORCE_INLINE explicit CharReservoir( std::basic_streambuf<value_type>* sink = nullptr ) noexcept
         : sink_ptr_{ sink }
      { }

      CBR_FORCE_INLINE auto pour( std::string_view view ) noexcept -> void
      {
         std::size_t const remaining = available( );
         //
         // 1. Check if the scratch buffer would overflow with this request.
         if ( bool const scratch_buffer_overflow = std::cmp_greater( view.size( ), remaining ); scratch_buffer_overflow )
         {
            // 1a. If a sink is available ...
            if ( sink_ptr_ != nullptr )
            {
               // ... flush whatever gathered so far and, ...
               drain( );
               //
               // ... if the view size is bigger than the scratch buffer capacity, directly put it in the sink.
               if ( view.size( ) > capacity( ) )
               {
                  put_in_sink( view.data( ), static_cast<std::streamsize>( view.size( ) ) );
                  return; // We early out here, because if we do not exhaust the view, we must copy it over to the scratch buffer later on.
               }
            }
            // 1b. If there isn't any sink available ...
            else
            {
               // ... adjust the view to fit the scratch buffer and mark overflowed.
               view.remove_suffix( view.size( ) - static_cast<size_t>( remaining ) );
               overflow_ = true;
            }
         }
         //
         // 2. Finally copy the view over to the scratch buffer. It is now guaranteed to fit in it, no UB.
         std::ranges::copy( view, std::next( scratch_buffer_.begin( ), static_cast<std::ptrdiff_t>( scratch_size_ ) ) );
         scratch_size_ += view.size( );
      }

      CBR_FORCE_INLINE auto pour( std::uint_least32_t value ) noexcept -> void
      {
         // std::uint_least32_t is guaranteed to hold at least 32 bits, so its maximum value is at least 4,294,967,295 (2³²−1), which is 10 digits.
         std::array<char, 10> digits;
         auto const [end, ec] = std::to_chars( digits.data( ), digits.data( ) + digits.size( ), value );
         pour( std::string_view{ digits.data( ), end } );
      }

      CBR_FORCE_INLINE auto pour( char c ) noexcept -> void
      {
         // If the scratch buffer isn't full, append the character (if it is not full, we certainly have at least one empty slot).
         if ( scratch_size_ < N ) [[likely]]
         {
            scratch_buffer_[scratch_size_++] = c;
            return;
         }
         //
         // Otherwise, the buffer is exactly full. If we have a sink to flush to, drain the scratch buffer ...
         if ( sink_ptr_ != nullptr )
         {
            drain( );
            scratch_buffer_[0] = c;
            scratch_size_ = 1;
         }
         else
         {
            overflow_ = true; // ... otherwise flag overflow.
         }
      }

      /**
       * Write the buffered characters to the sink stream and reset the buffer. Consumers should issue a final \p drain() after pouring to emit
       * whatever remained.
       *
       * @note No-op is bounded mode (no sink target).
       */
      CBR_FORCE_INLINE auto drain( ) noexcept -> void
      {
         if ( sink_ptr_ != nullptr && scratch_size_ > 0 )
         {
            put_in_sink( scratch_buffer_.data( ), static_cast<std::streamsize>( scratch_size_ ) );
            clear( );
         }
      }

      auto redirect( std::basic_streambuf<value_type>* sink ) noexcept -> void
      {
         sink_ptr_ = sink;
      }

      [[nodiscard]] CBR_FORCE_INLINE auto inserter( ) noexcept -> iterator_type { return iterator_type{ *this }; }

      [[nodiscard]] CBR_FORCE_INLINE auto view( ) const noexcept -> std::string_view { return { scratch_buffer_.data( ), scratch_size_ }; }

      [[nodiscard]] CBR_FORCE_INLINE static constexpr auto capacity( ) noexcept -> std::size_t { return N; }

      auto clear( ) noexcept -> void
      {
         scratch_size_ = 0;
         overflow_ = false;
      }

      [[nodiscard]] CBR_FORCE_INLINE auto overflowed( ) const noexcept -> bool { return overflow_; }

   private:
      std::array<value_type, N> scratch_buffer_;
      std::size_t scratch_size_ = 0;
      std::basic_streambuf<value_type>* sink_ptr_ = nullptr;
      bool overflow_ = false;

      [[nodiscard]] CBR_FORCE_INLINE auto available( ) const noexcept -> std::size_t { return capacity( ) - scratch_size_; }
      CBR_FORCE_INLINE auto put_in_sink( char const* s, std::streamsize n ) const noexcept -> void { sink_ptr_->sputn( s, n ); }
   };
}
