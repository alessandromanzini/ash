module;

#include <ash/pch.hpp>

#include <ash/cbridge.hpp>

export module ash:codec;


namespace ash::traits::detail
{
   template <typename T, typename... Ts> concept one_of = ( std::same_as<T, Ts> || ... );
}

export namespace ash::traits
{
   template <typename T> concept character = detail::one_of<std::remove_cv_t<T>, char, char8_t, char16_t, char32_t>;
}

export namespace ash::traits
{
   /**
    * Binary codec used by deferred dispatch: arguments are copied into the ring on the producer thread and rebuilt as formattable values on the
    * worker thread, which moves the formatting cost off the caller.
    *
    * A codec specialization exposes:
    *  - \c value_type:      the formattable type returned by \c deserialize. It may only refer to the serialized bytes, never to the source object.
    *  - \c fixed_size:      whether every payload is exactly \c max_serial_size bytes. Fixed records carry no length header and are never truncated.
    *  - \c max_serial_size: upper bound of a payload.
    *  - \c serial_size(v), \c serialize(dst, v, size), \c deserialize(src, size).
    *
    * @note Variable-size codecs must accept any \c size up to \c serial_size(v), since payloads are clipped to fit the message budget.
    */
   template <typename T> struct Codec
   {
      static_assert( false, "No Codec specialized for T..." ); // C++23 (P2593); fires only on instantiation
   };

   /**
    * Scalars are copied bit-wise. Only untyped pointers are accepted, since those are the only pointers std::format prints by address; anything
    * that would be dereferenced later (e.g. char*) must not be stored as a pointer.
    */
   template <typename T>
      requires std::is_arithmetic_v<T> || std::is_enum_v<T> || std::is_null_pointer_v<T> || detail::one_of<T, void*, void const*>
   struct Codec<T>
   {
      using value_type = T;

      static constexpr bool fixed_size{ true };
      static constexpr std::size_t max_serial_size{ sizeof( value_type ) };

      [[nodiscard]] static constexpr auto serial_size( value_type ) noexcept -> std::size_t { return max_serial_size; }

      static auto serialize( std::byte* dst, value_type value, std::size_t size ) noexcept -> void { std::memcpy( dst, &value, size ); }

      [[nodiscard]] static auto deserialize( std::byte const* src, std::size_t size ) noexcept -> value_type
      {
         value_type out{};
         std::memcpy( &out, src, size );
         return out;
      }
   };
}

namespace ash::traits::detail
{
   /**
    * Shared base for every string-like codec: the characters are copied, and the worker reads them back as a view into the message.
    */
   template <character C> struct StringCodec
   {
      static_assert( sizeof( C ) == 1, "Wide strings padding alignment is not implemented yet." );

      using value_type = std::basic_string_view<std::remove_cv_t<C>>;

      static constexpr bool fixed_size{ false };
      static constexpr std::size_t max_serial_size{ 255 * sizeof( C ) };

      [[nodiscard]] static auto serial_size( value_type view ) noexcept -> std::size_t { return view.size( ) * sizeof( C ); }

      static auto serialize( std::byte* dst, value_type view, std::size_t size ) noexcept -> void { std::memcpy( dst, view.data( ), size ); }

      [[nodiscard]] static auto deserialize( std::byte const* src, std::size_t size ) noexcept -> value_type
      {
         return { reinterpret_cast<std::remove_cv_t<C> const*>( src ), size / sizeof( C ) };
      }
   };
}

export namespace ash::traits
{
   template <character C> struct Codec<std::basic_string_view<C>> : detail::StringCodec<C>
   { };

   template <character C> struct Codec<std::basic_string<C>> : detail::StringCodec<C>
   { };

   // Covers both C const* and mutable C*. The terminator is not stored: the worker reads the characters back as a view.
   template <character C> struct Codec<C*> : detail::StringCodec<C>
   {
      [[nodiscard]] static auto serial_size( C const* cstr ) noexcept -> std::size_t
      {
         return cstr == nullptr ? 0 : std::char_traits<std::remove_cv_t<C>>::length( cstr ) * sizeof( C );
      }

      static auto serialize( std::byte* dst, C const* cstr, std::size_t size ) noexcept -> void
      {
         if ( size > 0 )
         {
            std::memcpy( dst, cstr, size );
         }
      }
   };

   // Arrays are read up to the first terminator (string literals and partially filled char buffers alike), never past N.
   template <character C, std::size_t N> struct Codec<C[N]> : detail::StringCodec<C>
   {
      [[nodiscard]] static auto serial_size( C const ( &buffer )[N] ) noexcept -> std::size_t
      {
         return static_cast<std::size_t>( std::ranges::find( buffer, C{} ) - std::ranges::begin( buffer ) ) * sizeof( C );
      }

      static auto serialize( std::byte* dst, C const ( &buffer )[N], std::size_t size ) noexcept -> void { std::memcpy( dst, buffer, size ); }
   };
}

namespace ash::traits::detail
{
   template <std::size_t length>
   using length_t = std::conditional_t<
     length <= std::numeric_limits<std::uint8_t>::max( ), std::uint8_t,
     std::conditional_t<length <= std::numeric_limits<std::uint16_t>::max( ), std::uint16_t, std::uint32_t>>;

   template <typename T> struct CodecTraits
   {
      using codec_type = Codec<std::remove_cvref_t<T>>;
      using value_type = codec_type::value_type;
      using length_type = length_t<codec_type::max_serial_size>; // header stores the payload size only

      // We need a header only if the size is not fixed.
      static constexpr std::size_t header_size{ codec_type::fixed_size != 0 ? sizeof( length_type ) : 0 };

      // Bytes that must be reserved no matter the argument value: the whole payload when fixed, the header alone otherwise.
      static constexpr std::size_t fixed_footprint{ codec_type::fixed_size ? codec_type::max_serial_size : header_size };
   };
}

export namespace ash
{
   /**
    * Serializes an argument pack into a flat byte record ("burn") and rebuilds formattable values from it ("feed").
    *
    * Layout, per argument in order: fixed-size codecs write their raw payload, variable-size codecs write a length header followed by the payload.
    * Variable payloads share whatever budget is left once every fixed footprint has been reserved, first come first served, and are clipped when it
    * runs out.
    *
    * @tparam Ts Decayed argument types, each with a \c traits::Codec specialization.
    */
   template <typename... Ts> class Burner final
   {
      template <typename T> using codec_traits_t = traits::detail::CodecTraits<T>;

   public:
      static constexpr std::size_t fixed_footprint{ ( std::size_t{} + ... + codec_traits_t<Ts>::fixed_footprint ) };

      Burner( ) = delete;

      /**
       * @return { bytes written, truncated }.
       */
      [[nodiscard]] static auto burn( std::span<std::byte> dst, Ts const&... args ) noexcept -> std::pair<std::size_t, bool>
      {
         assert( dst.size( ) >= fixed_footprint && "Destination cannot hold the fixed-size records." );
         //
         std::size_t cursor{ 0 };
         std::size_t budget{ dst.size( ) - fixed_footprint };
         bool truncated{ false };
         ( burn_single<Ts>( dst.data( ), cursor, budget, truncated, args ), ... );
         return { cursor, truncated };
      }

      template <std::invocable<typename codec_traits_t<Ts>::value_type const&...> F>
      static auto feed( std::span<std::byte const> src, F&& f ) noexcept( std::is_nothrow_invocable_v<F, typename codec_traits_t<Ts>::value_type const&...> )
        -> decltype( auto )
      {
         std::size_t cursor{ 0 };
         //
         // Braced initialization guarantees left-to-right evaluation, so the records are read back in the order they were burnt.
         std::tuple<typename codec_traits_t<Ts>::value_type...> const values{ read<Ts>( src.data( ), cursor )... };
         return std::apply( std::forward<F>( f ), values );
      }

   private:
      template <typename T>
      CBR_FORCE_INLINE static auto burn_single( std::byte* dst, std::size_t& cursor, std::size_t& budget, bool& truncated, T const& arg ) noexcept -> void
      {
         using traits_t = codec_traits_t<T>;
         using codec_t = traits_t::codec_type;
         //
         if constexpr ( codec_t::fixed_size )
         {
            codec_t::serialize( dst + cursor, arg, codec_t::max_serial_size );
            cursor += codec_t::max_serial_size;
         }
         else
         {
            // 1. Clip the payload to both the codec limit and the remaining budget.
            std::size_t const wanted{ codec_t::serial_size( arg ) };
            std::size_t const size{ std::min( { wanted, codec_t::max_serial_size, budget } ) };
            truncated = truncated || size < wanted;
            budget -= size;
            //
            // 2. Prepend the payload size for later deserialization.
            auto const header = static_cast<typename traits_t::length_type>( size );
            std::memcpy( dst + cursor, &header, sizeof( header ) );
            cursor += sizeof( header );
            //
            // 3. Finalize the data write serializing the argument.
            codec_t::serialize( dst + cursor, arg, size );
            cursor += size;
         }
      }

      template <typename T> [[nodiscard]] static auto read( std::byte const* src, std::size_t& cursor ) noexcept -> codec_traits_t<T>::value_type
      {
         using traits_t = codec_traits_t<T>;
         using codec_t = traits_t::codec_type;
         //
         std::size_t size{ codec_t::max_serial_size };
         if constexpr ( not codec_t::fixed_size )
         {
            typename traits_t::length_type header;
            std::memcpy( &header, src + cursor, sizeof( header ) );
            cursor += sizeof( header );
            size = header;
         }
         //
         std::byte const* const payload{ src + cursor };
         cursor += size;
         return codec_t::deserialize( payload, size );
      }
   };
}
