#include <ash/pch.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <latch>
#include <limits>

#if defined( __APPLE__ )
   #include <sys/sysctl.h>
#endif

import ash;


// Measures what one log call costs the thread that makes it, for every logger configuration, and prints the results as Markdown tables.
//
//   codec_bench            full run
//   codec_bench --quick    fewer bursts, for a smoke test
//
// Results go to stdout; progress goes to stderr and is erased when done, so `codec_bench > results.md` produces a clean file.

namespace
{
   // ---------------------------------------------------------------------------------------------------------------------------------------------
   // Settings
   // ---------------------------------------------------------------------------------------------------------------------------------------------

   struct Settings
   {
      std::size_t bursts{ 10'000 };                   // per thread and case
      std::size_t burst_length{ 32 };                 // calls timed together
      std::size_t warmup_calls{ 2'048 };              // untimed, to allocate rings and fault pages
      std::chrono::microseconds pause{ 200 };         // between bursts, so no ring ever fills
      std::size_t contended_threads{ 4 };
   };

   enum class Workload : std::uint8_t { bursty, hot };

   // ---------------------------------------------------------------------------------------------------------------------------------------------
   // Sink: discards output, counts entries
   // ---------------------------------------------------------------------------------------------------------------------------------------------

   /**
    * Discards everything, counting entries by their trailer (a single "\n" written in bulk after each body).
    */
   class CountingSink final : public std::streambuf
   {
   public:
      [[nodiscard]] auto entries( ) const noexcept -> std::size_t { return entries_.load( std::memory_order_acquire ); }
      auto reset( ) noexcept -> void { entries_.store( 0, std::memory_order_release ); }

   protected:
      auto xsputn( char const* s, std::streamsize n ) -> std::streamsize override
      {
         if ( n == 1 && s[0] == '\n' )
         {
            entries_.fetch_add( 1, std::memory_order_relaxed );
         }
         return n;
      }

      auto overflow( int_type c ) -> int_type override { return c; }

   private:
      std::atomic<std::size_t> entries_{ 0 };
   };

   // ---------------------------------------------------------------------------------------------------------------------------------------------
   // Message kinds
   // ---------------------------------------------------------------------------------------------------------------------------------------------

   std::string const owned_string{ "an owned std::string payload" };

   struct Integers
   {
      static constexpr std::string_view name{ "Integers" };
      static constexpr std::string_view sample{ R"_(`"message number {} of {} ({})"` with three ints)_" };
      static auto log( auto& logger, int i ) -> void { logger.info( "message number {} of {} ({})", i, 1'000'000, i * 3 ); }
   };

   struct Floats
   {
      static constexpr std::string_view name{ "Floats" };
      static constexpr std::string_view sample{ R"_(`"x={:.3f} y={:.3f} z={:>10.2e}"` with three doubles)_" };
      static auto log( auto& logger, int i ) -> void { logger.info( "x={:.3f} y={:.3f} z={:>10.2e}", i * 0.5, i * 1.25, i * 7.0 ); }
   };

   struct Strings
   {
      static constexpr std::string_view name{ "Strings" };
      static constexpr std::string_view sample{ R"_(`"{} says {} to {}"` with a string literal, a `std::string` and a `std::string_view`)_" };
      static auto log( auto& logger, [[maybe_unused]] int i ) -> void
      {
         logger.info( "{} says {} to {}", "producer", owned_string, std::string_view{ "consumer" } );
      }
   };

   // ---------------------------------------------------------------------------------------------------------------------------------------------
   // Measurement
   // ---------------------------------------------------------------------------------------------------------------------------------------------

   struct Result
   {
      double p50;
      double p99;
      double delivered; // fraction of timed calls that reached the sink
   };

   auto pause_between_bursts( Workload workload, std::chrono::microseconds pause ) -> void
   {
      if ( workload == Workload::bursty )
      {
         std::this_thread::sleep_for( pause );
         return;
      }
      auto const until = std::chrono::steady_clock::now( ) + pause;
      while ( std::chrono::steady_clock::now( ) < until )
      { }
   }

   auto wait_for_entries( CountingSink const& sink, std::size_t expected ) -> void
   {
      auto const deadline = std::chrono::steady_clock::now( ) + std::chrono::seconds{ 1 };
      while ( sink.entries( ) < expected && std::chrono::steady_clock::now( ) < deadline )
      {
         std::this_thread::sleep_for( std::chrono::milliseconds{ 1 } );
      }
   }

   [[nodiscard]] auto percentile( std::vector<double>& samples, double p ) -> double
   {
      auto const index = static_cast<std::size_t>( p * static_cast<double>( samples.size( ) - 1 ) );
      std::ranges::nth_element( samples, samples.begin( ) + static_cast<std::ptrdiff_t>( index ) );
      return samples[index];
   }

   template <typename Kind, typename Preset>
   [[nodiscard]] auto measure( Preset preset, std::size_t threads, Workload workload, Settings const& settings ) -> Result
   {
      CountingSink sink{};
      std::ostream stream{ &sink };
      std::vector<double> samples{};
      std::mutex samples_lock{};
      {
         ash::Logger logger{ preset, { .log_stream = stream, .err_stream = stream, .trailer_content = "\n" } };
         //
         // Warm up, let the backend drain, then count only the timed calls.
         for ( std::size_t i{ 0 }; i < settings.warmup_calls; ++i )
         {
            Kind::log( logger, static_cast<int>( i ) );
         }
         wait_for_entries( sink, settings.warmup_calls );
         sink.reset( );
         //
         std::latch start{ static_cast<std::ptrdiff_t>( threads ) };
         std::vector<std::thread> pool{};
         for ( std::size_t t{ 0 }; t < threads; ++t )
         {
            pool.emplace_back( [&] {
               std::vector<double> local{};
               local.reserve( settings.bursts );
               start.arrive_and_wait( );
               for ( std::size_t burst{ 0 }; burst < settings.bursts; ++burst )
               {
                  auto const begin = std::chrono::steady_clock::now( );
                  for ( std::size_t i{ 0 }; i < settings.burst_length; ++i )
                  {
                     Kind::log( logger, static_cast<int>( i ) );
                  }
                  auto const elapsed = std::chrono::steady_clock::now( ) - begin;
                  local.push_back( std::chrono::duration<double, std::nano>( elapsed ).count( ) / static_cast<double>( settings.burst_length ) );
                  pause_between_bursts( workload, settings.pause );
               }
               std::lock_guard lock{ samples_lock };
               samples.insert( samples.end( ), local.begin( ), local.end( ) );
            } );
         }
         for ( auto& thread : pool )
         {
            thread.join( );
         }
      } // a deferred logger drains its rings before it is destroyed
      //
      auto const expected = static_cast<double>( threads * settings.bursts * settings.burst_length );
      return { percentile( samples, 0.50 ), percentile( samples, 0.99 ), static_cast<double>( sink.entries( ) ) / expected };
   }

   // ---------------------------------------------------------------------------------------------------------------------------------------------
   // Configurations
   // ---------------------------------------------------------------------------------------------------------------------------------------------

   struct Row
   {
      std::string logger;
      std::size_t threads;
      std::array<Result, 3> results; // Integers, Floats, Strings
   };

   class Progress final
   {
   public:
      explicit Progress( std::size_t total ) noexcept
         : total_{ total }
      { }

      auto step( std::string_view what ) -> void
      {
         ++done_;
         std::print( stderr, "\r\033[2K  [{:>2}/{}] {}", done_, total_, what );
         std::fflush( stderr );
      }

      auto clear( ) -> void
      {
         std::print( stderr, "\r\033[2K" );
         std::fflush( stderr );
      }

   private:
      std::size_t total_;
      std::size_t done_{ 0 };
   };

   template <typename Preset>
   [[nodiscard]] auto run_row( std::string_view name, Preset preset, std::size_t threads, Workload workload, Settings const& settings,
                               Progress& progress ) -> Row
   {
      Row row{ .logger = std::string{ name }, .threads = threads, .results = {} };
      auto const label = [&]( std::string_view kind ) {
         return std::format( "{} · {} thread{} · {} · {}", name, threads, threads > 1 ? "s" : "", kind,
                             workload == Workload::bursty ? "bursty" : "hot" );
      };
      progress.step( label( Integers::name ) );
      row.results[0] = measure<Integers>( preset, threads, workload, settings );
      progress.step( label( Floats::name ) );
      row.results[1] = measure<Floats>( preset, threads, workload, settings );
      progress.step( label( Strings::name ) );
      row.results[2] = measure<Strings>( preset, threads, workload, settings );
      return row;
   }

   [[nodiscard]] auto run_workload( Workload workload, Settings const& settings, Progress& progress ) -> std::vector<Row>
   {
      std::size_t const many{ settings.contended_threads };
      return {
         run_row( "direct_unsync", ash::policy::direct_unsync, 1, workload, settings, progress ),
         run_row( "direct_sync", ash::policy::direct_sync, 1, workload, settings, progress ),
         run_row( "direct_sync", ash::policy::direct_sync, many, workload, settings, progress ),
         run_row( "deferred_unsync", ash::policy::deferred_unsync, 1, workload, settings, progress ),
         run_row( "deferred_sync", ash::policy::deferred_sync, 1, workload, settings, progress ),
         run_row( "deferred_sync", ash::policy::deferred_sync, many, workload, settings, progress ),
         run_row( "deferred_ordered_sync", ash::policy::deferred_ordered_sync, 1, workload, settings, progress ),
         run_row( "deferred_ordered_sync", ash::policy::deferred_ordered_sync, many, workload, settings, progress ),
      };
   }

   constexpr std::size_t rows_per_workload{ 8 };

   // ---------------------------------------------------------------------------------------------------------------------------------------------
   // Environment
   // ---------------------------------------------------------------------------------------------------------------------------------------------

   [[nodiscard]] auto sysctl_string( [[maybe_unused]] char const* key ) -> std::string
   {
#if defined( __APPLE__ )
      std::size_t size{ 0 };
      if ( sysctlbyname( key, nullptr, &size, nullptr, 0 ) != 0 || size == 0 )
      {
         return {};
      }
      std::string value( size, '\0' );
      sysctlbyname( key, value.data( ), &size, nullptr, 0 );
      value.resize( std::strlen( value.c_str( ) ) );
      return value;
#else
      return {};
#endif
   }

   [[nodiscard]] auto platform( ) -> std::string
   {
      std::string cpu{ sysctl_string( "machdep.cpu.brand_string" ) };
      std::string os{ sysctl_string( "kern.osproductversion" ) };
      std::string const cores{ std::format( "{} logical cores", std::thread::hardware_concurrency( ) ) };
#if defined( __APPLE__ )
      os = "macOS " + os;
#elif defined( _WIN32 )
      os = "Windows";
#elif defined( __linux__ )
      os = "Linux";
#endif
      if ( cpu.empty( ) )
      {
         return std::format( "{} · {}", cores, os );
      }
      return std::format( "{} · {} · {}", cpu, cores, os );
   }

   [[nodiscard]] auto compiler( ) -> std::string
   {
#if defined( __clang__ )
      return std::format( "clang {}.{}.{}", __clang_major__, __clang_minor__, __clang_patchlevel__ );
#elif defined( __GNUC__ )
      return std::format( "gcc {}.{}.{}", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__ );
#elif defined( _MSC_VER )
      return std::format( "MSVC {}", _MSC_FULL_VER );
#else
      return "unknown compiler";
#endif
   }

   [[nodiscard]] constexpr auto is_release_build( ) noexcept -> bool
   {
#if defined( NDEBUG )
      return true;
#else
      return false;
#endif
   }

   /**
    * Smallest step steady_clock can observe, in nanoseconds.
    */
   [[nodiscard]] auto timer_resolution( ) -> double
   {
      double smallest{ std::numeric_limits<double>::max( ) };
      for ( int i{ 0 }; i < 1'000; ++i )
      {
         auto const a = std::chrono::steady_clock::now( );
         auto b = std::chrono::steady_clock::now( );
         while ( b == a )
         {
            b = std::chrono::steady_clock::now( );
         }
         smallest = std::min( smallest, std::chrono::duration<double, std::nano>( b - a ).count( ) );
      }
      return smallest;
   }

   // ---------------------------------------------------------------------------------------------------------------------------------------------
   // Output
   // ---------------------------------------------------------------------------------------------------------------------------------------------

   [[nodiscard]] auto format_ns( double ns ) -> std::string
   {
      if ( ns < 100.0 )
      {
         return std::format( "{:.1f}", ns );
      }
      auto const whole = static_cast<long long>( std::lround( ns ) );
      return whole < 1'000 ? std::format( "{}", whole ) : std::format( "{},{:03}", whole / 1'000, whole % 1'000 );
   }

   [[nodiscard]] auto format_count( std::size_t value ) -> std::string
   {
      std::string digits{ std::to_string( value ) };
      for ( auto i = static_cast<std::ptrdiff_t>( digits.size( ) ) - 3; i > 0; i -= 3 )
      {
         digits.insert( static_cast<std::size_t>( i ), 1, ',' );
      }
      return digits;
   }

   [[nodiscard]] auto format_delivered( Row const& row ) -> std::string
   {
      double lowest{ 1.0 };
      for ( Result const& result : row.results )
      {
         lowest = std::min( lowest, result.delivered );
      }
      return lowest >= 0.9995 ? "100%" : std::format( "{:.1f}%", lowest * 100.0 );
   }

   auto print_table( std::vector<Row> const& rows ) -> void
   {
      std::vector<std::array<std::string, 6>> cells{};
      cells.push_back( { "Logger", "Threads", "Integers", "Floats", "Strings", "Delivered" } );
      for ( Row const& row : rows )
      {
         std::array<std::string, 6> line{ row.logger, std::format( "{}", row.threads ), "", "", "", format_delivered( row ) };
         for ( std::size_t k{ 0 }; k < 3; ++k )
         {
            line[k + 2] = std::format( "{} / {}", format_ns( row.results[k].p50 ), format_ns( row.results[k].p99 ) );
         }
         cells.push_back( std::move( line ) );
      }
      //
      std::array<std::size_t, 6> width{};
      for ( auto const& line : cells )
      {
         for ( std::size_t c{ 0 }; c < 6; ++c )
         {
            width[c] = std::max( width[c], line[c].size( ) );
         }
      }
      //
      // Column 0 left-aligned, numbers right-aligned.
      auto const print_line = [&]( std::array<std::string, 6> const& line ) {
         std::print( "| {:<{}} |", line[0], width[0] );
         for ( std::size_t c{ 1 }; c < 6; ++c )
         {
            std::print( " {:>{}} |", line[c], width[c] );
         }
         std::println( "" );
      };
      print_line( cells.front( ) );
      std::print( "|{}|", std::string( width[0] + 2, '-' ) );
      for ( std::size_t c{ 1 }; c < 6; ++c )
      {
         std::print( "{}:|", std::string( width[c] + 1, '-' ) );
      }
      std::println( "" );
      for ( std::size_t r{ 1 }; r < cells.size( ); ++r )
      {
         print_line( cells[r] );
      }
   }

   auto print_header( Settings const& settings, double resolution ) -> void
   {
      std::println( "# ash benchmark" );
      std::println( "" );
      std::println( "Caller-side latency: how long one log call keeps the calling thread busy. Formatting and I/O done on a worker thread are not" );
      std::println( "included. Lower is better." );
      std::println( "" );
      std::println( "| | |" );
      std::println( "|---|---|" );
      std::println( "| Platform | {} |", platform( ) );
      std::println( "| Compiler | {} · C++26 · {} |", compiler( ), is_release_build( ) ? "Release" : "Debug" );
      std::println( "| Method | {} bursts of {} calls per thread and configuration, {} µs between bursts |", format_count( settings.bursts ),
                    settings.burst_length, settings.pause.count( ) );
      std::println( "| Timer | {:.1f} ns resolution, about {:.1f} ns per call over a burst |", resolution,
                    resolution / static_cast<double>( settings.burst_length ) );
      std::println( "| Sink | Every logger writes its default title and the message to an in-memory sink that discards it |" );
      if ( not is_release_build( ) )
      {
         std::println( "" );
         std::println( "> **Debug build.** These numbers are not representative; build with the release preset." );
      }
   }

   auto print_legend( Settings const& settings ) -> void
   {
      std::println( "## Reading the tables" );
      std::println( "" );
      std::println( "- Each cell is **median / 99th percentile**, in nanoseconds per call." );
      std::println( "- **Integers**: {}.", Integers::sample );
      std::println( "- **Floats**: {}.", Floats::sample );
      std::println( "- **Strings**: {}.", Strings::sample );
      std::println( "- **Threads**: with {}, that many threads log through the same logger at once.", settings.contended_threads );
      std::println( "- **Delivered**: share of timed calls that reached the sink, lowest of the three message kinds. A deferred logger drops entries" );
      std::println( "  when its ring is full." );
   }
}

auto main( int argc, char** argv ) -> int
{
   Settings settings{};
   for ( int i{ 1 }; i < argc; ++i )
   {
      if ( std::string_view{ argv[i] } == "--quick" )
      {
         settings.bursts = 1'000;
      }
      else
      {
         std::println( stderr, "usage: {} [--quick]", argv[0] );
         return 1;
      }
   }
   //
   double const resolution{ timer_resolution( ) };
   Progress progress{ 2 * rows_per_workload * 3 };
   std::vector<Row> const bursty{ run_workload( Workload::bursty, settings, progress ) };
   std::vector<Row> const hot{ run_workload( Workload::hot, settings, progress ) };
   progress.clear( );
   //
   print_header( settings, resolution );
   std::println( "" );
   print_legend( settings );
   std::println( "" );
   std::println( "## Bursty" );
   std::println( "" );
   std::println( "The thread sleeps between bursts, like an application that logs in spurts. Caches and clocks cool down in between." );
   std::println( "" );
   print_table( bursty );
   std::println( "" );
   std::println( "## Hot" );
   std::println( "" );
   std::println( "The thread busy-waits between bursts, keeping caches and clocks warm. This isolates the cost of the call itself." );
   std::println( "" );
   print_table( hot );
}
