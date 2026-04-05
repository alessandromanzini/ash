#include <mongoose/__module/dialog.hpp> // CHeck standard __dependencies plural/singular convention

#import <Cocoa/Cocoa.h>


namespace mongoose
{
   // [[nodiscard]] auto init_alert( char const* const title, Dialog::Style const style ) noexcept -> NSAlert*
   // {
   //    [NSApplication sharedApplication];
   //    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
   //    [NSApp activateIgnoringOtherApps:YES];
   //    //
   //    NSAlert* alert    = [[NSAlert alloc] init];
   //    alert.messageText = [NSString stringWithUTF8String:title];
   //    //
   //    switch( style )
   //    {
   //       case Dialog::Style::information:
   //       {
   //          alert.alertStyle = NSAlertStyleInformational;
   //          break;
   //       }
   //       //
   //       case Dialog::Style::warning:
   //       {
   //          alert.alertStyle = NSAlertStyleWarning;
   //          break;
   //       }
   //       //
   //       case Dialog::Style::critical:
   //       {
   //          alert.alertStyle = NSAlertStyleCritical;
   //          break;
   //       }
   //    }
   //    return alert;
   // }
   //
   //
   // [[nodiscard]] auto make_text_view( char const* const text, double const width ) noexcept -> NSView*
   // {
   //    NSTextField* field = [[NSTextField alloc] initWithFrame:NSMakeRect( 0., 0., width, 0. )];
   //    field.stringValue     = [NSString stringWithUTF8String:text];
   //    field.editable        = NO;
   //    field.bordered        = NO;
   //    field.drawsBackground = NO;
   //    field.bezeled         = NO;
   //    field.font            = [NSFont systemFontOfSize:NSFont.systemFontSize];
   //    //
   //    // auto size height to fit content
   //    [field sizeToFit];
   //    //
   //    return field;
   // }
   //
   //
   // Dialog::Dialog( std::string_view title, std::string_view body ) noexcept
   //    : title_( title )
   //    , body_( body )
   // {
   // }
   //
   //
   // auto Dialog::display( ) noexcept-> char const*
   // {
   //    NSAlert* alert = init_alert( title_.data( ), style_ );
   //    //
   //    [alert setAccessoryView:make_text_view( body_.data( ), width_ )];
   //    //
   //    for ( char const* const option : std::span{ options_.begin( ), options_count_ } )
   //    {
   //       [alert addButtonWithTitle:[NSString stringWithUTF8String:option]];
   //    }
   //    [alert buttons][0].keyEquivalent = @"";
   //    //
   //    NSButton* default_button = [alert buttons][default_option_.value_or( 0U )];
   //    default_button.keyEquivalent = @"\r";     // set return key (⏎) for default button
   //    //
   //    if ( cancel_option_.has_value( ) )
   //    {
   //       NSButton* cancel_button  = [alert buttons][cancel_option_.value( )];
   //       cancel_button.keyEquivalent = @"\033"; // set escape key for cancel button
   //    }
   //    //
   //    auto const result = [alert runModal];
   //    auto const index = static_cast<std::size_t>( result - NSAlertFirstButtonReturn );
   //    //
   //    return ( last_selected_option_ = options_[index] );
   // }
}
