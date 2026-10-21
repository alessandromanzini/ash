#include <ash/pch.hpp>

import ash;

// Null sink that counts emitted records.
struct CountingBuf final : std::streambuf
{
   std::atomic<std::size_t> records{ 0 };
   auto xsputn( char const* s, std::streamsize n ) -> std::streamsize override
   {
      if ( n == 2 && s[0] == '\n' && s[1] == '\n' )
      {
         records.fetch_add( 1, std::memory_order_relaxed );
      }
      return n;
   }
   auto overflow( int_type c ) -> int_type override { return c; }
};

// Caller-side cost per log call, sink silenced, worker running.
auto main( ) -> int
{
   CountingBuf null_sink{};
   std::streambuf* const prev = std::clog.rdbuf( &null_sink );
   constexpr int iterations = 1'000'000;
   constexpr int rounds = 10;
   std::string const owned{ "an owned std::string payload" };
   //
   auto const bench = [&]( std::string_view name, auto&& make_logger, auto&& body ) {
      std::array<double, rounds> ns{};
      null_sink.records = 0;
      for ( double& r : ns )
      {
         auto logger = make_logger( );
         auto const start = std::chrono::steady_clock::now( );
         for ( int i = 0; i < iterations; ++i )
         {
            body( *logger, i );
         }
         r = std::chrono::duration<double, std::nano>( std::chrono::steady_clock::now( ) - start ).count( ) / iterations;
      }
      std::ranges::sort( ns );
      double const delivered = 100.0 * static_cast<double>( null_sink.records.load( ) ) / ( iterations * rounds );
      std::println( "{:<24} median {:6.1f} ns/call  min {:6.1f}  delivered {:5.1f}%", name, ns[rounds / 2], ns[0], delivered );
   };
   //
   // Bursts of 32 (half the ring), waiting for the worker to drain in between: every timed call is a delivered call.
   auto const burst = [&]( std::string_view name, auto&& make_logger, auto&& body ) {
      constexpr std::size_t burst_len = 32;
      constexpr std::size_t bursts = 20'000;
      auto logger = make_logger( );
      std::vector<double> ns{};
      ns.reserve( bursts );
      null_sink.records = 0;
      for ( std::size_t b = 0; b < bursts; ++b )
      {
         auto const start = std::chrono::steady_clock::now( );
         for ( std::size_t i = 0; i < burst_len; ++i )
         {
            body( *logger, static_cast<int>( i ) );
         }
         ns.push_back( std::chrono::duration<double, std::nano>( std::chrono::steady_clock::now( ) - start ).count( ) / burst_len );
         while ( null_sink.records.load( ) < ( b + 1 ) * burst_len )
         { }
      }
      std::ranges::sort( ns );
      std::println(
        "{:<24} p50 {:6.1f} ns/call  p99 {:7.1f}  delivered {:5.1f}%", name, ns[bursts / 2], ns[bursts * 99 / 100],
        100.0 * static_cast<double>( null_sink.records.load( ) ) / ( bursts * burst_len ) );
   };
   auto const direct = [] {
      return std::make_unique<ash::Logger<ash::policy::Unsync, ash::policy::Direct>>( ash::policy::unsync, ash::policy::direct );
   };
   auto const deferred = [] {
      return std::make_unique<ash::Logger<ash::policy::Unsync, ash::policy::Deferred>>( ash::policy::unsync, ash::policy::deferred );
   };
   auto const ints = []( auto& l, int i ) { l.info( "benchmark message number {} of {} ({})", i, 1'000'000, i * 3 ); };
   auto const floats = []( auto& l, int i ) { l.info( "x={:.3f} y={:.3f} z={:>10.2e}", i * 0.5, i * 1.25, i * 7.0 ); };
   auto const strs = [&]( auto& l, int i ) {
      l.info( "{} says {} to {}", "producer", owned, std::string_view{ "consumer" } );
      ( void )i;
   };
   //
   bench( "direct   ints", direct, ints );
   bench( "deferred ints", deferred, ints );
   bench( "direct   floats+specs", direct, floats );
   bench( "deferred floats+specs", deferred, floats );
   bench( "direct   strings", direct, strs );
   bench( "deferred strings", deferred, strs );
   //
   std::println( "-- bursts --" );
   burst( "direct   ints", direct, ints );
   burst( "deferred ints", deferred, ints );
   burst( "direct   floats+specs", direct, floats );
   burst( "deferred floats+specs", deferred, floats );
   burst( "direct   strings", direct, strs );
   burst( "deferred strings", deferred, strs );
   std::clog.rdbuf( prev );
}
