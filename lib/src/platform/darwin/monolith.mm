#include "ash/module/monolith.hpp"
#include "ash/module/theme.hpp"

#include <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#pragma mark - UI Constants

namespace ash::ui
{
   constexpr CGFloat kPadding             = 24.0;
   constexpr CGFloat kTitleHeightOffset   = 18.0;
   constexpr CGFloat kSignalIconSize      = 11.0;
   constexpr CGFloat kSignalFontSize      = 9.0;
   constexpr CGFloat kSignalBorderWidth   = 0.5;
   constexpr CGFloat kSignalRadius        = 4.0;
   constexpr CGFloat kTitleSpacing        = 10.0;
   constexpr CGFloat kTitleFontSize       = 13.0;
   constexpr CGFloat kToolTipFontSize     = 11.0;
   constexpr CGFloat kToolTipIconWidth    = 23.0;
   constexpr CGFloat kToolTipIconHeight   = 16.0;
   constexpr CGFloat kToolTipPaddingX     = 20.0;
   constexpr CGFloat kToolTipPaddingY     = 12.0;
   constexpr CGFloat kButtonRadius        = 6.0;
   constexpr CGFloat kButtonHeight        = 32.0;
   constexpr CGFloat kButtonWidth         = 120.0;
   constexpr CGFloat kSpacing             = 10.0;
   constexpr CGFloat kContentSpacing      = 16.0;
}

#pragma mark - Objective-C Runtime Helpers

@interface MG_ButtonHandler : NSObject <NSWindowDelegate>
@property (nonatomic) NSInteger tag;
@property (nonatomic) NSInteger cancel_tag;
@end

@implementation MG_ButtonHandler

- (void)handleClick:(id)sender
{
   [NSApp stopModalWithCode:self.tag];
}

- (void)windowWillClose:(NSNotification*)n
{
   [NSApp stopModalWithCode:self.cancel_tag];
}

@end

@interface MG_Panel : NSPanel
@end

@implementation MG_Panel

- ( BOOL )performKeyEquivalent:( NSEvent* )event
{
   if ( event.type != NSEventTypeKeyDown )
   {
      return [super performKeyEquivalent:event];
   }
   //
   if ( ![[event charactersIgnoringModifiers] isEqualToString:@"\r"] )
   {
      return [super performKeyEquivalent:event];
   }
   //
   NSNumber* tag_number = objc_getAssociatedObject( self, "master_tag" );
   if ( !tag_number )
   {
      return [super performKeyEquivalent:event];
   }
   //
   NSInteger target_tag  = tag_number.integerValue;
   NSMutableArray* stack = [NSMutableArray arrayWithObject:self.contentView];
   while ( stack.count )
   {
      NSView* v = stack.lastObject;
      [stack removeLastObject];
      //
      if ( [v isKindOfClass:[NSButton class]] )
      {
         NSButton* b = static_cast<NSButton*>( v );
         if ( b.tag == target_tag )
         {
            [b performClick:nil];
            return YES;
         }
      }
      //
      for ( NSView* s in v.subviews )
      {
         [stack addObject:s];
      }
   }
   return [super performKeyEquivalent:event];
}

@end

// Auto-sizing attachment that matches font line height automatically
@interface MG_SignalAttachment : NSTextAttachment
@end

@implementation MG_SignalAttachment

- (CGRect)attachmentBoundsForTextContainer:(NSTextContainer *)textContainer
                     proposedLineFragment:(CGRect)lineFrag
                            glyphPosition:(CGPoint)position
                           characterIndex:(NSUInteger)charIndex
{
   NSLayoutManager* lm = textContainer.layoutManager;
   NSTextStorage* storage = lm.textStorage;
   //
   if ( !storage || charIndex >= storage.length )
   {
      return self.bounds;
   }
   //
   NSFont* font = [storage attribute:NSFontAttributeName
                              atIndex:charIndex
                       effectiveRange:NULL];
   //
   if ( !font )
   {
      return self.bounds;
   }
   //
   const CGFloat height  = ceil( font.ascender - font.descender );
   const CGFloat yOffset = font.descender;
   //
   return NSMakeRect( 0.0, yOffset, height, height );
}

@end

@interface MG_LabelCell : NSTextFieldCell
@end

@implementation MG_LabelCell

- (NSRect)drawingRectForBounds:(NSRect)rect
{
   NSRect r = [super drawingRectForBounds:rect];
   r.origin.y    = 0.0;
   r.size.height = rect.size.height;
   return r;
}

@end

@interface MG_TooltipPopover : NSPopover
@property (nonatomic, assign) NSControl* owner;
@end

@implementation MG_TooltipPopover

- (void)mouseDown:(NSEvent*)event
{
   if ( !self.owner ) { return; }
   [self.owner mouseDown:event];
}

@end

@interface MG_HoverButton : NSButton
@property (nonatomic, strong) NSColor* tint;
@property (nonatomic, strong) NSString* clipboard_text;
@property (nonatomic, strong) NSPopover* tooltip_popover;
@property (nonatomic, strong) NSTextField* tooltip_label;
@property (nonatomic, assign) BOOL suppress_tooltip;
@end

@implementation MG_HoverButton

- (void)updateTrackingAreas
{
   [super updateTrackingAreas];
   for ( NSTrackingArea* a in self.trackingAreas ) { [self removeTrackingArea:a]; }
   //
   NSTrackingArea* area = [[NSTrackingArea alloc]
   initWithRect:self.bounds
        options:NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways
          owner:self
       userInfo:nil];
   [self addTrackingArea:area];
}

- (void) showTooltip
{
   MG_TooltipPopover* popover = [[MG_TooltipPopover alloc] init];
   popover.owner              = self;
   //
   NSViewController* vc         = [[NSViewController alloc] init];
   self.tooltip_label           = [NSTextField labelWithString:@"Copy log to clipboard [⌘C]"];
   self.tooltip_label.font      = [NSFont systemFontOfSize:ash::ui::kToolTipFontSize];
   self.tooltip_label.alignment = NSTextAlignmentCenter;
   self.tooltip_label.translatesAutoresizingMaskIntoConstraints = NO;
   //
   NSView* view = [[NSView alloc] initWithFrame:NSZeroRect];
   [view addSubview:self.tooltip_label];
   [NSLayoutConstraint activateConstraints:@[
      [self.tooltip_label.topAnchor      constraintEqualToAnchor:view.topAnchor      constant:ash::ui::kToolTipPaddingY],
      [self.tooltip_label.bottomAnchor   constraintEqualToAnchor:view.bottomAnchor   constant:-ash::ui::kToolTipPaddingY],
      [self.tooltip_label.leadingAnchor  constraintEqualToAnchor:view.leadingAnchor  constant:ash::ui::kToolTipPaddingX],
      [self.tooltip_label.trailingAnchor constraintEqualToAnchor:view.trailingAnchor constant:-ash::ui::kToolTipPaddingX]
   ]];
   //
   vc.view                       = view;
   popover.contentViewController = vc;
   popover.behavior              = NSPopoverBehaviorApplicationDefined;
   popover.animates              = NO;
   //
   self.tooltip_popover = popover;
   [popover showRelativeToRect:self.bounds ofView:self preferredEdge:NSRectEdgeMinY];
}

- (void)mouseEntered:(NSEvent*)event
{
   self.layer.backgroundColor = [self.tint colorWithAlphaComponent:0.1].CGColor;
   //
   if ( self.tooltip_popover || self.suppress_tooltip ) { return; }
   [self showTooltip];
}

- (void)mouseExited:(NSEvent*)event
{
   self.layer.backgroundColor = [NSColor clearColor].CGColor;
   [self.tooltip_popover close];
   self.tooltip_popover = nil;
}

- (void)mouseDown:(NSEvent*)event
{
   [super mouseDown:event];
   //
   if ( !self.tooltip_popover || !self.tooltip_label ) { return; }
   //
   NSImage* checkmark = [NSImage imageWithSystemSymbolName:@"checkmark.circle.fill" accessibilityDescription:nil];
   NSImageSymbolConfiguration* cfg = [NSImageSymbolConfiguration configurationWithPointSize:ash::ui::kToolTipFontSize
                                                    weight:NSFontWeightMedium
                                                     scale:NSImageSymbolScaleSmall];
   checkmark = [checkmark imageWithSymbolConfiguration:cfg];
   //
   NSTextAttachment* attach = [[NSTextAttachment alloc] init];
   attach.image = checkmark;
   //
   NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
   style.alignment = NSTextAlignmentCenter;
   style.lineBreakMode = NSLineBreakByClipping;
   //
   NSAttributedString* icon = [NSAttributedString attributedStringWithAttachment:attach];
   NSMutableAttributedString* str = [[NSMutableAttributedString alloc] initWithString:@"Copied "
      attributes:@{
         NSParagraphStyleAttributeName: style,
         NSFontAttributeName: self.tooltip_label.font
      }];
   [str appendAttributedString:icon];
   //
   self.tooltip_label.attributedStringValue = str;
}

- (void)copyToClipboard:(id)sender
{
   if ( !self.clipboard_text ) { return; }
   NSPasteboard* pb = [NSPasteboard generalPasteboard];
   [pb clearContents];
   [pb setString:self.clipboard_text forType:NSPasteboardTypeString];
}

@end

#pragma mark - Conversions

namespace ash::ui
{
   [[nodiscard]] auto to_alignment( style::TextAlignment const a ) noexcept -> NSTextAlignment
   {
      switch ( a )
      {
         default:
         case style::TextAlignment::left     : return NSTextAlignmentLeft;
         case style::TextAlignment::center   : return NSTextAlignmentCenter;
         case style::TextAlignment::right    : return NSTextAlignmentRight;
         case style::TextAlignment::justified: return NSTextAlignmentJustified;
      }
   }

   [[nodiscard]] auto to_color( Signal const s ) noexcept -> NSColor*
   {
      const auto uint_to_float = []( uint8_t const channel ) { return static_cast<CGFloat>( channel ) / UINT8_MAX; };
      const theme::Color color = theme::signal_to_color( s );
      return [NSColor colorWithRed:uint_to_float( color.r )
                             green:uint_to_float( color.g )
                              blue:uint_to_float( color.b )
                             alpha:uint_to_float( color.a )];
   }

   [[nodiscard]] auto to_symbol( Signal const s ) noexcept -> NSString*
   {
      switch (s)
      {
         default:
         case Signal::trace   : return @"info.bubble";
         case Signal::info    : return @"info.circle.fill";
         case Signal::debug   : return @"ant.fill";
         case Signal::warning : return @"exclamationmark.triangle.fill";
         case Signal::error   : return @"xmark.octagon.fill";
         case Signal::fatal   : return @"bolt.trianglebadge.exclamationmark.fill";
      }
   }

   [[nodiscard]] auto make_font( CGFloat const size, style::Font const style ) noexcept -> NSFont*
   {
      switch ( style )
      {
         default:
         case style::Font::normal:
            return [NSFont systemFontOfSize:size];

         case style::Font::bold:
            return [NSFont boldSystemFontOfSize:size];

         case style::Font::italic:
            return [[NSFontManager sharedFontManager]
               convertFont:[NSFont systemFontOfSize:size]
               toHaveTrait:NSItalicFontMask];
      }
   }
}

#pragma mark - Element Builders

namespace ash::ui
{
   [[nodiscard]] auto tint_image( NSImage* image, NSColor* color ) noexcept -> NSImage*
   {
      if ( !image ) { return nil; }
      //
      NSImage* const img = [image copy];
      //
      [img lockFocus];
      [color set];
      //
      const NSRect rect = NSMakeRect( 0.0, 0.0, img.size.width, img.size.height );
      NSRectFillUsingOperation( rect, NSCompositingOperationSourceAtop );
      //
      [img unlockFocus];
      //
      return img;
   }

   [[nodiscard]] auto make_label( const Inscription& f ) noexcept -> NSTextField*
   {
      NSTextField* label = [[NSTextField alloc] initWithFrame:NSZeroRect];
      //
      label.editable        = NO;
      label.bordered        = NO;
      label.bezeled         = NO;
      label.drawsBackground = NO;
      label.lineBreakMode   = NSLineBreakByWordWrapping;
      //
      NSMutableParagraphStyle* p = [[NSMutableParagraphStyle alloc] init];
      p.alignment                = to_alignment( f.alignment );
      //
      label.attributedStringValue =
         [[NSAttributedString alloc]
            initWithString:[NSString stringWithUTF8String:f.content.data( )]
            attributes:@{
               NSFontAttributeName: make_font( static_cast<CGFloat>( f.font_size ), f.font_style ),
               NSParagraphStyleAttributeName: p
            }];
      //
      return label;
   }

   [[nodiscard]] auto make_button( const char* text, NSInteger tag ) noexcept -> NSButton*
   {
      NSButton* b           = [[NSButton alloc] initWithFrame:NSZeroRect];
      b.title               = [NSString stringWithUTF8String:text];
      b.bezelStyle          = NSBezelStyleGlass;
      b.contentTintColor    = [NSColor controlAccentColor];
      b.tag                 = tag;
      b.wantsLayer          = YES;
      b.layer.masksToBounds = YES;
      [b sizeToFit];
      return b;
   }

   [[nodiscard]] auto make_sig_badge( Signal const sig ) noexcept -> NSTextField*
   {
      NSTextField* label = [[NSTextField alloc] initWithFrame:NSZeroRect];
      [label setCell:[[MG_LabelCell alloc] init]];
      label.editable              = NO;
      label.bordered              = NO;
      label.drawsBackground       = NO;
      label.bezeled               = NO;
      label.alignment             = NSTextAlignmentCenter;
      label.wantsLayer            = YES;
      label.layer.cornerRadius    = kSignalRadius;
      label.layer.borderWidth     = kSignalBorderWidth;
      label.layer.borderColor     = ui::to_color( sig ).CGColor;
      label.layer.backgroundColor = [ui::to_color( sig ) colorWithAlphaComponent:0.1].CGColor;
      label.translatesAutoresizingMaskIntoConstraints = NO;
      //
      // ICON
      NSImage* icon = [NSImage imageWithSystemSymbolName:ui::to_symbol( sig ) accessibilityDescription:nil];
      NSImageSymbolConfiguration* cfg =
         [NSImageSymbolConfiguration configurationWithPointSize:kSignalIconSize weight:NSFontWeightRegular
                                                          scale:NSImageSymbolScaleSmall];
      //
      icon = [icon imageWithSymbolConfiguration:cfg];
      icon = tint_image( icon, ui::to_color( sig ) );
      //
      NSFont* const badge_font = [NSFont systemFontOfSize:kSignalFontSize weight:NSFontWeightSemibold];
      const CGFloat glyph_h    = std::ceil( badge_font.ascender - badge_font.descender );
      const CGFloat baseline   = badge_font.descender;
      //
      MG_SignalAttachment* attach = [[MG_SignalAttachment alloc] init];
      attach.image  = icon;
      attach.bounds = NSMakeRect( 0.0, baseline, glyph_h, glyph_h );
      //
      NSAttributedString* icon_str = [NSAttributedString attributedStringWithAttachment:attach];
      //
      // TEXT
      NSString* text      = [NSString stringWithUTF8String:reflection::to_string( sig, true ).data( )];
      NSDictionary* attrs = @{
         NSFontAttributeName: badge_font,
         NSForegroundColorAttributeName: ui::to_color( sig )
      };
      //
      NSAttributedString* text_str        = [[NSAttributedString alloc] initWithString:text attributes:attrs];
      NSMutableAttributedString* combined = [[NSMutableAttributedString alloc] initWithString:@" "];
      [combined appendAttributedString:icon_str];
      [combined appendAttributedString:[[NSAttributedString alloc] initWithString:@" "]];
      [combined appendAttributedString:text_str];
      [combined appendAttributedString:[[NSAttributedString alloc] initWithString:@" "]];
      //
      const CGFloat badge_h = std::ceil( badge_font.ascender - badge_font.descender ) + kSignalFontSize;
      [label.heightAnchor constraintEqualToConstant:badge_h].active = YES;
      //
      label.attributedStringValue = combined;
      //
      return label;
   }
}

#pragma mark - Layout

namespace ash::ui
{
   [[nodiscard]] auto make_text_stack( std::span<const Inscription> fields, CGFloat const width ) noexcept -> std::pair<NSStackView*, CGFloat>
   {
      NSStackView* stack = [NSStackView stackViewWithViews:@[]];
      stack.orientation  = NSUserInterfaceLayoutOrientationVertical;
      stack.alignment    = NSLayoutAttributeLeading;
      stack.spacing      = kSpacing;
      //
      CGFloat height = 0.0;
      //
      for ( const auto& f : fields )
      {
         NSTextField* label = make_label( f );
         label.translatesAutoresizingMaskIntoConstraints = NO;
         label.preferredMaxLayoutWidth = width;
         //
         [label sizeToFit];
         [label.widthAnchor constraintEqualToConstant:width].active = YES;
         //
         [stack addArrangedSubview:label];
         height += label.frame.size.height;
      }
      //
      if ( fields.size( ) > 1U )
      {
         height += kSpacing * static_cast<CGFloat>(fields.size( ) - 1U);
      }
      //
      return { stack, height };
   }

   [[nodiscard]] auto make_button_row( std::span<const Choice> choices ) noexcept -> NSStackView*
   {
      NSStackView* row = [NSStackView stackViewWithViews:@[]];
      row.orientation  = NSUserInterfaceLayoutOrientationHorizontal;
      row.alignment    = NSLayoutAttributeCenterY;
      row.spacing      = kSpacing;
      //
      for ( NSInteger i = 0; i < static_cast<NSInteger>( choices.size( ) ); ++i )
      {
         NSButton* b = make_button( choices[static_cast<size_t>( i )].label.data( ), i);
         b.translatesAutoresizingMaskIntoConstraints                   = NO;
         [b.widthAnchor constraintEqualToConstant:kButtonWidth].active = YES;
         [row addArrangedSubview:b];
      }
      //
      return row;
   }
}

#pragma mark - Panel

namespace ash::ui
{
   [[nodiscard]] auto make_panel(
       char const* const title, CGFloat const width, CGFloat const height, Signal const sig,
       NSMutableString* combined_text_content ) noexcept -> MG_Panel*
   {
      MG_Panel* const panel = [[MG_Panel alloc]
         initWithContentRect:NSMakeRect( 0.0, 0.0, width, height )
         styleMask:NSWindowStyleMaskTitled            |
                   NSWindowStyleMaskClosable          |
                   NSWindowStyleMaskMiniaturizable    |
                   NSWindowStyleMaskFullSizeContentView
         backing:NSBackingStoreBuffered
         defer:NO];
      [[panel standardWindowButton:NSWindowZoomButton] setHidden:YES];
      //
      panel.titleVisibility            = NSWindowTitleHidden;
      panel.titlebarAppearsTransparent = YES;
      panel.movableByWindowBackground  = YES;
      panel.backgroundColor            = [NSColor clearColor];
      panel.opaque                     = NO;
      panel.hasShadow                  = YES;
      panel.becomesKeyOnlyIfNeeded     = NO;
      //
      NSView* titlebar_view    = [[panel standardWindowButton:NSWindowCloseButton] superview];
      NSStackView* title_stack = [NSStackView stackViewWithViews:@[]];
      title_stack.orientation  = NSUserInterfaceLayoutOrientationHorizontal;
      title_stack.alignment    = NSLayoutAttributeCenterY;
      title_stack.spacing      = kTitleSpacing;
      title_stack.translatesAutoresizingMaskIntoConstraints = NO;
      //
      {
         NSTextField* sig_label = make_sig_badge( sig );
         [title_stack addArrangedSubview:sig_label];
         //
         // TITLE
         NSTextField* titleText    = [[NSTextField alloc] initWithFrame:NSZeroRect];
         titleText.stringValue     = [NSString stringWithUTF8String:title];
         titleText.editable        = NO;
         titleText.bordered        = NO;
         titleText.drawsBackground = NO;
         titleText.bezeled         = NO;
         titleText.alignment       = NSTextAlignmentLeft;
         titleText.textColor       = ui::to_color( sig );
         titleText.font            = [NSFont systemFontOfSize:kTitleFontSize weight:NSFontWeightSemibold];
         titleText.translatesAutoresizingMaskIntoConstraints = NO;
         //
         [title_stack addArrangedSubview:titleText];
         //
         [titlebar_view addSubview:title_stack];
         //
         NSButton* miniaturize_button = [panel standardWindowButton:NSWindowMiniaturizeButton];
         [NSLayoutConstraint activateConstraints:@[
             [title_stack.leadingAnchor constraintEqualToAnchor:miniaturize_button.trailingAnchor constant:kContentSpacing],
             [title_stack.centerYAnchor constraintEqualToAnchor:miniaturize_button.centerYAnchor]
         ]];
      }
      //
      {
         // COPY BUTTON
         NSImage* copy_icon = [NSImage imageWithSystemSymbolName:@"doc.on.doc" accessibilityDescription:@"Copy"];
         NSImageSymbolConfiguration* cfg =
             [NSImageSymbolConfiguration configurationWithPointSize:kSignalIconSize
                                                             weight:NSFontWeightRegular
                                                              scale:NSImageSymbolScaleSmall];
         //
         copy_icon = [copy_icon imageWithSymbolConfiguration:cfg];
         [copy_icon setTemplate:YES];
         //
         MG_HoverButton* copy_button  = [[MG_HoverButton alloc] initWithFrame:NSZeroRect];
         copy_button.image            = copy_icon;
         copy_button.tint             = ui::to_color( sig );
         copy_button.bordered         = NO;
         copy_button.translatesAutoresizingMaskIntoConstraints = NO;
         //
         [copy_button.image setTemplate:YES];
         [copy_button.widthAnchor constraintEqualToConstant:kToolTipIconWidth].active = YES;
         [copy_button.heightAnchor constraintEqualToConstant:kToolTipIconHeight].active = YES;
         copy_button.contentTintColor = ui::to_color( sig );
         //
         copy_button.wantsLayer         = YES;
         copy_button.layer.cornerRadius = kButtonRadius;
         copy_button.layer.borderWidth  = kSignalBorderWidth;
         copy_button.layer.borderColor  = ui::to_color( sig ).CGColor;
         //
         copy_button.keyEquivalent             = @"c";
         copy_button.keyEquivalentModifierMask = NSEventModifierFlagCommand;
         //
         copy_button.imagePosition = NSImageOnly;
         //
         copy_button.target         = copy_button;
         copy_button.action         = @selector(copyToClipboard:);
         copy_button.clipboard_text = combined_text_content;
         //
         [titlebar_view addSubview:copy_button];
         //
         [NSLayoutConstraint activateConstraints:@[
            [copy_button.trailingAnchor constraintEqualToAnchor:titlebar_view.trailingAnchor constant:-kPadding],
            [copy_button.centerYAnchor constraintEqualToAnchor:title_stack.centerYAnchor]
         ]];
      }
      //
      NSVisualEffectView* const vfx = [[NSVisualEffectView alloc] initWithFrame:NSMakeRect( 0.0, 0.0, width, height )];
      vfx.material                  = NSVisualEffectMaterialHUDWindow;
      vfx.blendingMode              = NSVisualEffectBlendingModeBehindWindow;
      vfx.state                     = NSVisualEffectStateActive;
      vfx.wantsLayer                = YES;
      vfx.layer.masksToBounds       = YES;
      //
      panel.contentView = vfx;
      //
      return panel;
   }
}

#pragma mark - Monolith

namespace ash
{
   auto Monolith::manifest( ) noexcept -> Choice
   {
      using namespace ui;
      //
      [NSApplication sharedApplication];
      [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
      [NSApp activateIgnoringOtherApps:YES];
      //
      const auto max_w = static_cast<CGFloat>( max_modal_width_ );
      const auto min_w = static_cast<CGFloat>( min_width_.value_or( 0.0f ) );
      //
      const CGFloat buttons_w =
         static_cast<CGFloat>( choices_count_ ) * kButtonWidth +
         static_cast<CGFloat>( choices_count_ - 1U ) * kSpacing + kPadding * 2.0;
      //
      const CGFloat panel_w = std::min( std::max( min_w, buttons_w ), max_w );
      const CGFloat inner_w = panel_w - kPadding * 2.0;
      //
      const auto [text_stack, text_h] = make_text_stack( std::span{ inscriptions_.begin( ), inscriptions_count_ }, inner_w );
      //
      NSStackView* buttons = make_button_row( std::span{ choices_.begin( ), choices_count_ } );
      NSArray*     btn_view = buttons.views;
      //
      NSButton* master             = btn_view[master_choice_.value_or( 0U )];
      master.layer.backgroundColor = to_color( signal_ ).CGColor;
      master.layer.cornerRadius    = kButtonRadius;
      //
      if ( cancel_choice_ ) { [btn_view[cancel_choice_.value( )] setKeyEquivalent:@"\033"]; }
      //
      NSMutableArray* handlers = [NSMutableArray array];
      //
      for ( NSButton* b in btn_view )
      {
         MG_ButtonHandler* h = [[MG_ButtonHandler alloc] init];
         h.tag               = b.tag;
         b.target            = h;
         b.action            = @selector( handleClick: );
         [handlers addObject:h];
      }
      //
      NSStackView* content = [NSStackView stackViewWithViews:@[text_stack, buttons]];
      content.orientation  = NSUserInterfaceLayoutOrientationVertical;
      content.alignment    = NSLayoutAttributeCenterX;
      content.spacing      = kContentSpacing;
      content.edgeInsets   = NSEdgeInsetsMake( kPadding, kPadding, kPadding, kPadding );
      //
      NSMutableString* combined_text_content = [NSMutableString string];
      for ( const auto& f : std::span{ inscriptions_.begin( ), inscriptions_count_ } )
      {
         if ( !f.content.empty( ) )
         {
            [combined_text_content appendString:[NSString stringWithUTF8String:f.content.data( )]];
            [combined_text_content appendString:@"\n"];
         }
      }
      //
      const CGFloat panel_h       = kTitleHeightOffset + text_h + kButtonHeight + kContentSpacing + kPadding * 2.0;
      MG_Panel* panel             = make_panel( title_.data( ), panel_w, panel_h, signal_, combined_text_content );
      panel.initialFirstResponder = panel.contentView;
      [panel center];
      [panel.contentView addSubview:content];
      //
      objc_setAssociatedObject( panel, "master_tag", @( master.tag ), OBJC_ASSOCIATION_RETAIN_NONATOMIC );
      content.translatesAutoresizingMaskIntoConstraints = NO;
      //
      [NSLayoutConstraint activateConstraints:@[
         [content.centerYAnchor  constraintEqualToAnchor:panel.contentView.centerYAnchor constant:kTitleHeightOffset * 0.5],
         [content.leadingAnchor  constraintEqualToAnchor:panel.contentView.leadingAnchor],
         [content.trailingAnchor constraintEqualToAnchor:panel.contentView.trailingAnchor]
      ]];
      //
      {
         const auto close_tag = static_cast<NSInteger>( cancel_choice_.value_or( choices_count_ - 1U ) );
         MG_ButtonHandler* delegate = handlers[0];
         delegate.cancel_tag        = close_tag;
         panel.delegate             = delegate;
      }
      //
      // Ensure the app is fully active before running the modal, otherwise the
      // panel may appear desaturated/unfocused if called early at startup.
      [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
      [panel makeKeyAndOrderFront:nil];
      [panel makeKeyWindow];
      const auto selection = static_cast<size_t>( [NSApp runModalForWindow:panel] );
      [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
      //
      return selection < choices_count_ ? choices_[selection] : Choice{ };
   }
}