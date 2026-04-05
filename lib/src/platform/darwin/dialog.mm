#include <mongoose/__module/dialog.hpp>

#include <AppKit/AppKit.h>
#import  <Cocoa/Cocoa.h>


// -------------------------------------------------------------------------
// BUTTON HANDLER
// -------------------------------------------------------------------------
@interface MG_ButtonHandler : NSObject <NSWindowDelegate>
@property (nonatomic) NSInteger tag;
@property (nonatomic) NSInteger cancel_tag;
@end

@implementation MG_ButtonHandler
- (void)handleClick:(id)sender         { [NSApp stopModalWithCode:self.tag];        }
- (void)windowWillClose:(NSNotification*)n { [NSApp stopModalWithCode:self.cancel_tag]; }
@end


namespace mongoose
{
   // -------------------------------------------------------------------------
   // CONSTANTS
   // -------------------------------------------------------------------------
   namespace
   {
      constexpr CGFloat padding              = 26.;
      constexpr CGFloat title_h              = 15.;
      constexpr CGFloat button_h             = 32.;
      constexpr CGFloat button_w             = 120.;
      constexpr CGFloat spacing              = 10.;
      constexpr CGFloat text_button_spacing  = 20.;
   }


   // -------------------------------------------------------------------------
   // CONVERSIONS
   // -------------------------------------------------------------------------
   [[nodiscard]] auto to_ns_text_alignment( DialogTextAlignment const alignment ) noexcept -> NSTextAlignment
   {
      switch ( alignment )
      {
         default:
         case DialogTextAlignment::left:      return NSTextAlignmentLeft;
         case DialogTextAlignment::center:    return NSTextAlignmentCenter;
         case DialogTextAlignment::right:     return NSTextAlignmentRight;
         case DialogTextAlignment::justified: return NSTextAlignmentJustified;
      }
   }


   // -------------------------------------------------------------------------
   // NS MAKERS
   // -------------------------------------------------------------------------
   [[nodiscard]] auto make_font( CGFloat const font_size, DialogFontStyle const font_style ) noexcept -> NSFont*
   {
      switch ( font_style )
      {
         default:
         case DialogFontStyle::normal: return [NSFont systemFontOfSize:font_size];
         case DialogFontStyle::bold:   return [NSFont boldSystemFontOfSize:font_size];
         case DialogFontStyle::italic:
            return [[NSFontManager sharedFontManager]
               convertFont:[NSFont systemFontOfSize:font_size]
               toHaveTrait:NSItalicFontMask];
      }
   }


   [[nodiscard]] auto make_label( DialogTextField const& field ) noexcept -> NSTextField*
   {
      NSTextField* const label = [[NSTextField alloc] initWithFrame:NSZeroRect];
      //
      label.editable        = NO;
      label.bordered        = NO;
      label.drawsBackground = NO;
      label.bezeled         = NO;
      label.lineBreakMode   = NSLineBreakByWordWrapping;
      //
      NSMutableParagraphStyle* const para = [[NSMutableParagraphStyle alloc] init];
      para.alignment = to_ns_text_alignment( field.alignment );
      //
      label.attributedStringValue = [[NSAttributedString alloc]
         initWithString:[NSString stringWithUTF8String:field.content.data( )]
         attributes:@{
            NSFontAttributeName           : make_font( static_cast<CGFloat>( field.font_size ), field.font_style ),
            NSParagraphStyleAttributeName : para,
         }];
      //
      return label;
   }


   [[nodiscard]] auto make_button( char const* const title, NSInteger const tag ) noexcept -> NSButton*
   {
      NSButton* const button = [[NSButton alloc] initWithFrame:NSZeroRect];
      //
      button.title               = [NSString stringWithUTF8String:title];
      button.bezelStyle          = NSBezelStyleGlass;
      button.wantsLayer          = YES;
      button.layer.masksToBounds = YES;
      button.contentTintColor    = [NSColor controlAccentColor];
      button.tag                 = tag;
      //
      [button sizeToFit];
      return button;
   }


   [[nodiscard]] auto make_panel( char const* const title, CGFloat const width, CGFloat const height ) noexcept -> NSPanel*
   {
      [NSApplication sharedApplication];
      [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
      [NSApp activateIgnoringOtherApps:YES];
      //
      NSPanel* const panel = [[NSPanel alloc]
         initWithContentRect:NSMakeRect( 0., 0., width, height )
         styleMask:NSWindowStyleMaskTitled            |
                   NSWindowStyleMaskClosable          |
                   NSWindowStyleMaskMiniaturizable    |
                   NSWindowStyleMaskFullSizeContentView
         backing:NSBackingStoreBuffered
         defer:NO];
      //
      [[panel standardWindowButton:NSWindowZoomButton] setHidden:YES];
      //
      panel.title                      = [NSString stringWithUTF8String:title];
      panel.titlebarAppearsTransparent = YES;
      panel.movableByWindowBackground  = YES;
      panel.backgroundColor            = [NSColor clearColor];
      panel.opaque                     = NO;
      panel.hasShadow                  = YES;
      panel.becomesKeyOnlyIfNeeded     = NO;
      //
      NSVisualEffectView* const vfx = [[NSVisualEffectView alloc] initWithFrame:NSMakeRect( 0., 0., width, height )];
      //
      vfx.material            = NSVisualEffectMaterialHUDWindow;
      vfx.blendingMode        = NSVisualEffectBlendingModeBehindWindow;
      vfx.state               = NSVisualEffectStateActive;
      vfx.wantsLayer          = YES;
      vfx.layer.masksToBounds = YES;
      //
      panel.contentView = vfx;
      //
      [panel makeKeyAndOrderFront:nil];
      [panel center];
      //
      return panel;
   }


   // -------------------------------------------------------------------------
   // LAYOUT HELPERS
   // -------------------------------------------------------------------------
   [[nodiscard]] auto make_text_stack( std::span<DialogTextField const> fields, CGFloat const inner_w ) noexcept -> std::pair<NSStackView*, CGFloat>
   {
      NSStackView* const stack = [NSStackView stackViewWithViews:@[]];
      stack.orientation        = NSUserInterfaceLayoutOrientationVertical;
      stack.alignment          = NSLayoutAttributeLeading;
      stack.spacing            = spacing;
      //
      CGFloat total_h = 0.;
      //
      for ( DialogTextField const& field : fields )
      {
         NSTextField* const label = make_label( field );
         //
         label.translatesAutoresizingMaskIntoConstraints = NO;
         label.preferredMaxLayoutWidth                   = inner_w;
         //
         [label sizeToFit];
         [label.widthAnchor constraintEqualToConstant:inner_w].active = YES;
         //
         [stack addArrangedSubview:label];
         total_h += label.frame.size.height;
      }
      //
      if ( fields.size( ) > 1 )
      {
         total_h += spacing * static_cast<CGFloat>( fields.size( ) - 1 );
      }
      //
      return { stack, total_h };
   }


   [[nodiscard]] auto make_button_row( std::span<char const* const> options, CGFloat const offset_w ) noexcept -> NSStackView*
   {
      NSStackView* const row = [NSStackView stackViewWithViews:@[]];
      row.orientation        = NSUserInterfaceLayoutOrientationHorizontal;
      row.distribution       = NSStackViewDistributionGravityAreas;
      row.alignment          = NSLayoutAttributeCenterY;
      row.spacing            = offset_w;
      //
      for ( NSInteger i = 0; i < static_cast<NSInteger>( options.size( ) ); ++i )
      {
         NSButton* const btn = make_button( options[static_cast<size_t>( i )], i );
         btn.translatesAutoresizingMaskIntoConstraints = NO;
         [btn.widthAnchor constraintEqualToConstant:button_w].active = YES;
         [row addView:btn inGravity:NSStackViewGravityCenter];
      }
      //
      return row;
   }


   // -------------------------------------------------------------------------
   // DIALOG
   // -------------------------------------------------------------------------
   auto Dialog::display( ) noexcept -> std::string_view
   {
      auto const max_w     = static_cast<CGFloat>( max_modal_width_ );
      auto const min_w     = static_cast<CGFloat>( min_width_.value_or( 0.f ) );
      auto const buttons_w = static_cast<CGFloat>( options_count_ ) * button_w + static_cast<CGFloat>( options_count_ - 1U ) * spacing + padding * 2.;
      auto const panel_w   = std::min( std::max( { min_w, buttons_w } ), max_w );
      auto const inner_w   = panel_w - padding * 2.;
      //
      // 1. text stack
      auto const [text_stack, text_stack_h] = make_text_stack( std::span{ text_fields_.begin( ), text_field_count_ }, inner_w );
      //
      // 2. button row + key equivalents
      NSStackView* const button_row = make_button_row( std::span{ options_.begin( ), options_count_ }, spacing );
      //
      NSArray* const buttons = button_row.views;
      //
      [buttons[default_option_.value_or( 0U )] setKeyEquivalent:@"\r"];
      //
      if ( cancel_option_.has_value( ) )
      {
         [buttons[cancel_option_.value( )] setKeyEquivalent:@"\033"];
      }
      //
      // 3. wire button handlers
      NSMutableArray* const handlers = [NSMutableArray arrayWithCapacity:buttons.count];
      //
      for ( NSButton* btn in buttons )
      {
         MG_ButtonHandler* const handler = [[MG_ButtonHandler alloc] init];
         //
         handler.tag = btn.tag;
         btn.target  = handler;
         btn.action  = @selector(handleClick:);
         [handlers addObject:handler];
      }
      //
      // 4. content stack
      NSStackView* const content = [NSStackView stackViewWithViews:@[ text_stack, button_row ]];
      content.orientation        = NSUserInterfaceLayoutOrientationVertical;
      content.alignment          = NSLayoutAttributeCenterX;
      content.spacing            = text_button_spacing;
      content.edgeInsets         = NSEdgeInsetsMake( padding, padding, padding, padding );
      //
      [content setHuggingPriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationHorizontal];
      [content setHuggingPriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationVertical];
      //
      // 5. panel
      CGFloat const panel_h = text_stack_h + button_h + text_button_spacing + padding * 2.;
      //
      NSPanel* const panel = make_panel( title_.data( ), panel_w, panel_h );
      [panel.contentView addSubview:content];
      //
      content.translatesAutoresizingMaskIntoConstraints = NO;
      [NSLayoutConstraint activateConstraints:@[
         [content.topAnchor      constraintEqualToAnchor:panel.contentView.topAnchor    constant:title_h],
         [content.bottomAnchor   constraintEqualToAnchor:panel.contentView.bottomAnchor],
         [content.leadingAnchor  constraintEqualToAnchor:panel.contentView.leadingAnchor],
         [content.trailingAnchor constraintEqualToAnchor:panel.contentView.trailingAnchor],
      ]];
      //
      // 6. delegate for X button
      NSInteger const close_tag        = static_cast<NSInteger>( cancel_option_.value_or( options_count_ - 1U ) );
      MG_ButtonHandler* const delegate = handlers[0];
      delegate.cancel_tag              = close_tag;
      panel.delegate                   = delegate;
      //
      // 7. run modal
      auto const selection = static_cast<size_t>( [NSApp runModalForWindow:panel] );
      return selection < options_count_ ? options_[selection] : "";
   }
}