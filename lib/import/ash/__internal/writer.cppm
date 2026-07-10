module;

#include <ash/pch.hpp>

#include <ash/cbridge.hpp>

export module ash:writer;

import :format;
import :policy;
import :signal;
import :write_config;


export namespace ash
{


   /**
    * Owns the sink configuration and the compiled title format. It is deliberately thread-agnostic and optimized to write both title and body as
    * bulk \c sputn calls.
    *
    * @note Title formatting is optimized by compiling the title format rather than using a generalized \c std::format call.
    */
   class Writer final
   {
   public:
      explicit Writer( schema::WriteConfig config ) noexcept
         : config_{ std::move( config ) }
      {
         title_format_ = CompiledFormat{ config_.title_format };
      }

      template <size_t inline_buffer_size> CBR_FORCE_INLINE auto emit( schema::WriteMetadata const& metadata, std::string_view body ) const noexcept -> void
      {
         std::streambuf* const buf = config_.log_stream.get( ).rdbuf( );
         //
         if ( config_.title_injection == schema::WriteConfig::TitleInjection::per_block )
         {
            CharReservoir<char, inline_buffer_size> reservoir{ buf };
            title_format_.render( reservoir, metadata );
            reservoir.drain( );
            buf->sputc( '\n' );
         }
         //
         buf->sputn( body.data( ), static_cast<std::streamsize>( body.size( ) ) );
         buf->sputn( config_.trailer_content.c_str( ), static_cast<std::streamsize>( config_.trailer_content.size( ) ) );
      }

   private:
      schema::WriteConfig const config_;
      CompiledFormat title_format_;
   };
}
