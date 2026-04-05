#include <mongoose/__module/library.hpp>

#include <mongoose/cbridge.hpp>
#include <mongoose/__module/dialog.hpp>


namespace mongoose
{
   auto log( std::source_location const source_location ) -> void
   {
      std::string const where = std::format(
         "{} at {}:{}:{}", source_location.function_name( ), source_location.file_name( ), source_location.line( ),
         source_location.column( ) );

      auto res = Dialog{ "Assertion" }
                 .with_text_field(
                    {
                       .content = "Log manager needs a default property."
                    } )
                 .with_text_field(
                    {
                       .content = where,
                       .font_size = 10.f,
                       .font_style = DialogFontStyle::italic,
                       .alignment = DialogTextAlignment::left
                    } )
                 .with_option( "Ignore" )
                 .with_option( "Mute", Dialog::OptionTag::primary )
                 .with_option( "Debug" )
                 .with_option( "Exit", Dialog::OptionTag::cancel )
                 .display( );
      std::print( "Jakub likes minors: {}", res );
   }
}


// enum class Level{ warning };
//
// struct log_handle
// {
//    Level level;
//    std::source_location source_location;
//
//    template <typename... TArgs>
//    void log(std::format_string<TArgs...> fmt, TArgs&&... args)
//    {
//       std::print(fmt, std::forward<TArgs>(args)...);
//    }
// };
//
// struct Logger
// {
//    log_handle open(Level level, std::source_location source_location = std::source_location::current())
//    {
//       return { level, source_location };
//    }
// };
//
//
// Logger logger{};
// logger.open(Level::warning).log("Lick my ball sack: {}", 1U);
