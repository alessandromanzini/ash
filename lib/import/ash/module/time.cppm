module;

#include <ash/pch.hpp>

#include <ash/cbridge.hpp>

export module ash:time;


namespace ash::detail
{
   template <typename Precision> constexpr bool is_hours_only_v = std::ratio_equal_v<typename Precision::period, std::chrono::hours::period>;
   template <typename Precision> constexpr bool is_minutes_only_v = std::ratio_equal_v<typename Precision::period, std::chrono::minutes::period>;
   template <typename Precision> constexpr bool is_sub_minute_v = std::ratio_less_equal_v<typename Precision::period, std::chrono::seconds::period>;
}

export namespace ash::traits
{
   template <typename P> concept time_precision = detail::is_hours_only_v<P> || detail::is_minutes_only_v<P> || detail::is_sub_minute_v<P>;
}

namespace ash::detail
{
   /**
    * Width of the fixed rendered format for \p Precision: "HH", "HH:MM" or "HH:MM:SS[.x]".
    * @note Each branch mirrors a write from \c time::render_hms, which asserts against this total.
    * @tparam Precision
    */
   template <traits::time_precision Precision>
   constexpr std::size_t rendered_size_v = [] -> std::size_t {
      std::size_t size{ 2 }; // HH
      if constexpr ( not is_hours_only_v<Precision> )
      {
         size += 3; // :MM
      }
      if constexpr ( is_sub_minute_v<Precision> )
      {
         size += 3; // :SS
         if constexpr ( constexpr std::size_t decimals = std::chrono::hh_mm_ss<Precision>::fractional_width; decimals > 0 )
         {
            size += 1 + decimals; // .x
         }
      }
      return size;
   }( );
}

export namespace ash::time
{
   /**
    * A stack-only, trivially-copyable fixed-width string.
    * @tparam Precision
    */
   template <traits::time_precision Precision> struct TimeString
   {
      using hms_type = std::chrono::hh_mm_ss<Precision>;

      static constexpr std::size_t repr_length = detail::rendered_size_v<Precision>;
      static constexpr std::size_t fractional_width = hms_type::fractional_width;
      static constexpr bool has_decimals = detail::is_sub_minute_v<Precision> && fractional_width > 0;

      std::array<char, repr_length> chars{};

      [[nodiscard]] static constexpr auto size( ) noexcept -> std::size_t { return repr_length; }
      [[nodiscard]] constexpr auto begin( ) noexcept -> char* { return chars.data( ); }
      [[nodiscard]] constexpr auto begin( ) const noexcept -> char const* { return chars.data( ); }
      [[nodiscard]] constexpr auto end( ) noexcept -> char* { return chars.data( ) + size( ); }
      [[nodiscard]] constexpr auto end( ) const noexcept -> char const* { return chars.data( ) + size( ); }

      [[nodiscard]] constexpr operator std::string_view( ) const noexcept { return view( ); }
      [[nodiscard]] constexpr auto view( ) const noexcept -> std::string_view { return std::string_view{ chars.data( ), repr_length }; }
   };
   static_assert( std::is_trivially_copyable_v<TimeString<std::chrono::microseconds>>, "TimeString must remain trivially copyable!" );
}

namespace ash::detail
{
   /**
    * Two decimals table ranging from "00" to "99".
    */
   constexpr auto two_digit_table = [] {
      std::array<char, 200> table{};
      for ( std::size_t i = 0; i < 100; ++i )
      {
         table[i * 2] = static_cast<char>( '0' + i / 10 );
         table[i * 2 + 1] = static_cast<char>( '0' + i % 10 );
      }
      return table;
   }( );

   /**
    * Writes \p value as exactly two ASCII digits using a quick table lookup.
    * @param out
    * @param value Must be less than 100.
    * @return The new head of the \c char stream.
    */
   CBR_FORCE_INLINE constexpr auto write_two_digit( char* out, std::size_t value ) noexcept -> char*
   {
      assert( value < 100 && "write_two_digit: value must have at most two digits!" );
      out[0] = two_digit_table[value * 2];
      out[1] = two_digit_table[value * 2 + 1];
      return std::next( out, 2 );
   }

   /**
    * Writes \p value as exactly \p Width zero-padded ASCII digits, least-significant last.
    * @tparam width
    * @param out
    * @param value Must be less than 10^Width.
    * @return The new head of the \c char stream.
    */
   template <std::size_t width> CBR_FORCE_INLINE constexpr auto write_decimals( char* out, std::size_t value ) noexcept -> char*
   {
      char* const tail = std::next( out, width );
      for ( char* it = tail; it != out; value /= 10 )
      {
         *--it = static_cast<char>( '0' + value % 10 );
      }
      assert( value == 0 && "write_decimals: value must fit in Width digits!" );
      return tail;
   }
}

export namespace ash::time
{
   /**
    * @brief Renders the time-of-day of \p timestamp as fixed-width "HH[:MM[:SS[.x]]]" using quick table lookups.
    *
    * @tparam Precision
    * @tparam Clock
    * @tparam Duration
    * @param timestamp
    * @return
    */
   template <traits::time_precision Precision = std::chrono::microseconds, typename Clock, typename Duration>
   constexpr auto render_hms( std::chrono::time_point<Clock, Duration> timestamp ) noexcept -> TimeString<Precision>
   {
      std::chrono::time_point<Clock, Precision> const precision_timestamp = std::chrono::floor<Precision>( timestamp );
      typename TimeString<Precision>::hms_type const hms{ precision_timestamp - std::chrono::floor<std::chrono::days>( precision_timestamp ) };
      //
      TimeString<Precision> repr{};
      //
      char* cursor = repr.begin( );
      auto const append_char = [&cursor]( char c ) { *cursor++ = c; };
      //
      cursor = detail::write_two_digit( cursor, static_cast<std::size_t>( hms.hours( ).count( ) ) );
      //
      if constexpr ( not detail::is_hours_only_v<Precision> )
      {
         append_char(':');
         cursor = detail::write_two_digit( cursor, static_cast<std::size_t>( hms.minutes( ).count( ) ) );
      }
      if constexpr ( detail::is_sub_minute_v<Precision> )
      {
         append_char(':');
         cursor = detail::write_two_digit( cursor, static_cast<std::size_t>( hms.seconds( ).count( ) ) );
         //
         if constexpr ( TimeString<Precision>::has_decimals )
         {
         append_char('.');
            cursor =
              detail::write_decimals<TimeString<Precision>::fractional_width>( cursor, static_cast<std::size_t>( hms.subseconds( ).count( ) ) );
         }
      }
      //
      assert( cursor == repr.end( ) && "render_hms: rendered characters must match TimeString::repr_length!" );
      return repr;
   }
}
