#ifndef MONGOOSE_DIALOG_HPP
#define MONGOOSE_DIALOG_HPP

#include <mongoose/pch.hpp>


namespace mongoose
{
   // +--------------------------------+
   // | ENUMERATIONS                   |
   // +--------------------------------+
   enum class DialogFontStyle : uint8_t
   {
      normal, bold, italic
   };

   enum class DialogTextAlignment : uint8_t
   {
      left, center, right, justified
   };

   // +--------------------------------+
   // | DIALOG TEXT FIELD              |
   // +--------------------------------+
   struct DialogTextField
   {
      std::string_view    content{};
      float               font_size  = 16.f;
      DialogFontStyle     font_style = DialogFontStyle::normal;
      DialogTextAlignment alignment  = DialogTextAlignment::left;
   };

   // +--------------------------------+
   // | DIALOG                         |
   // +--------------------------------+
   class Dialog final
   {
   public:
      enum class Style : uint8_t
      {
         information, warning, critical
      };

      enum class OptionTag : uint8_t
      {
         none, primary, cancel
      };

   public:
      explicit Dialog( std::string_view title ) noexcept;
      ~Dialog( ) noexcept = default;

      Dialog( Dialog const& )                        = delete;
      Dialog( Dialog&& ) noexcept                    = default;
      auto operator=( Dialog const& ) -> Dialog&     = delete;
      auto operator=( Dialog&& ) noexcept -> Dialog& = delete;

      // auto set_icon( DialogIcon const icon ) -> void;

      auto set_minimum_width( this auto&& self, float width ) noexcept -> decltype(self);

      auto with_text_field( this auto&& self, DialogTextField const& field_info ) noexcept -> decltype(self);
      auto with_option( this auto&& self, std::string_view option, OptionTag tag = OptionTag::none ) noexcept -> decltype(self);

      auto display( ) noexcept -> std::string_view;

   private:
      static constexpr float max_modal_width_ = 900.f;

      std::string_view const title_;

      std::optional<float> min_width_{};

      static constexpr size_t                             max_accessories_count_ = 6U;
      std::array<DialogTextField, max_accessories_count_> text_fields_{}; // make in_place_vector
      std::array<char const*, max_accessories_count_>     options_{};

      uint8_t                text_field_count_ = 0U;
      uint8_t                options_count_    = 0U;
      std::optional<uint8_t> default_option_{};
      std::optional<uint8_t> cancel_option_{};
   };


   inline Dialog::Dialog( std::string_view const title ) noexcept : title_{ title } { }


   auto Dialog::set_minimum_width( this auto&& self, float width ) noexcept -> decltype(self)
   {
      self.min_width_ = width;
      return self;
   }


   auto Dialog::with_text_field( this auto&& self, DialogTextField const& field_info ) noexcept -> decltype(self)
   {
      if ( self.text_field_count_ < self.max_accessories_count_ )
      {
         self.text_fields_[self.text_field_count_++] = field_info;
      }
      return self;
   }


   auto Dialog::with_option( this auto&& self, std::string_view option, OptionTag const tag ) noexcept -> decltype(self)
   {
      if ( self.options_count_ >= self.max_accessories_count_ || option.empty( ) )
      {
         return self;
      }
      //
      self.options_[self.options_count_] = option.data( );
      //
      switch ( tag )
      {
         case OptionTag::none: break;
         //
         case OptionTag::primary:
         {
            self.default_option_ = self.options_count_;
            break;
         }
         //
         case OptionTag::cancel:
         {
            self.cancel_option_ = self.options_count_;
            break;
         }
      }
      //
      ++self.options_count_;
      //
      return self;
   }
}


#endif //!MONGOOSE_DIALOG_HPP
