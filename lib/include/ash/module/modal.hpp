#ifndef ASH_MONOLITH_HPP
#define ASH_MONOLITH_HPP

#include <ash/pch.hpp>

#include <ash/config/signal.hpp>


namespace ash
{
   // ───[[ STYLES ]]──────────────────────────────────────────────────────────────
   namespace style
   {
      enum class Font : uint8_t { normal, bold, italic };
      enum class TextAlignment : uint8_t { left, center, right, justified };
   }

   // ───[[ REMARK ]]──────────────────────────────────────────────────────────────
   struct Remark // TODO: Allow override for alignment
   {
      static constexpr float header_font_size  = 16.0f;
      static constexpr float body_font_size    = 14.0f;
      static constexpr float caption_font_size = 10.0f;

      std::string_view     content;
      float                font_size  = body_font_size;
      style::Font          font_style = style::Font::normal;
      style::TextAlignment alignment  = style::TextAlignment::left;

      [[nodiscard]] static auto as_header( std::string_view const content ) noexcept -> Remark
      {
         return { .content = content, .font_size = header_font_size };
      }

      [[nodiscard]] static auto as_header_bold( std::string_view const content ) noexcept -> Remark
      {
         return { .content = content, .font_size = header_font_size, .font_style = style::Font::bold };
      }

      [[nodiscard]] static auto as_body( std::string_view const content ) noexcept -> Remark
      {
         return { .content = content, .font_size = body_font_size };
      }

      [[nodiscard]] static auto as_caption( std::string_view const content ) noexcept -> Remark
      {
         return { .content = content, .font_size = caption_font_size, .font_style = style::Font::italic };
      }
   };

   // ───[[ CHOICE ]]──────────────────────────────────────────────────────────────
   struct Choice
   {
      std::string_view label;
      uint8_t          token                                = UINT8_MAX;
      enum class Tag : uint8_t { none, master, cancel } tag = Tag::none;

      [[nodiscard]] static auto as_master( std::string_view const label, uint8_t const token = UINT8_MAX ) noexcept -> Choice
      {
         return { .label = label, .token = token, .tag = Tag::master };
      }

      [[nodiscard]] static auto as_cancel( std::string_view const label, uint8_t const token = UINT8_MAX ) noexcept -> Choice
      {
         return { .label = label, .token = token, .tag = Tag::cancel };
      }
   };

   // ───[[ MODAL ]]───────────────────────────────────────────────────────────────
   class Modal final
   {
   public:
      explicit Modal( std::string_view title ) noexcept;
      ~Modal( ) noexcept = default;

      Modal( Modal const& )                        = delete;
      Modal( Modal&& ) noexcept                    = default;
      auto operator=( Modal const& ) -> Modal&     = delete;
      auto operator=( Modal&& ) noexcept -> Modal& = delete;

      [[nodiscard]] auto set_minimum_width( this auto&& self, float width ) noexcept -> decltype( self );
      [[nodiscard]] auto set_signal( this auto&& self, Signal sig ) noexcept -> decltype( self );

      [[nodiscard]] auto with_remark( this auto&& self, Remark const& remark ) noexcept -> decltype( self );
      [[nodiscard]] auto with_choice( this auto&& self, Choice const& choice ) noexcept -> decltype( self );

      auto raise( ) noexcept -> Choice;

   private:
      static constexpr float max_modal_width_ = 900.0f;

      std::string_view const title_;

      std::optional<float> min_width_;
      Signal               signal_ = Signal::info;

      static constexpr size_t                    max_accessories_count_ = 6U;
      std::array<Remark, max_accessories_count_> remarks_{}; // use inplace_vector
      std::array<Choice, max_accessories_count_> choices_{};

      uint8_t                remarks_count_ = 0U;
      uint8_t                choices_count_ = 0U;
      std::optional<uint8_t> master_choice_;
      std::optional<uint8_t> cancel_choice_;
   };

   inline Modal::Modal( std::string_view const title ) noexcept
      : title_{ title }
   { }

   auto Modal::set_minimum_width( this auto&& self, float width ) noexcept -> decltype( self )
   {
      self.min_width_ = width;
      return self;
   }

   auto Modal::set_signal( this auto&& self, Signal sig ) noexcept -> decltype( self )
   {
      self.signal_ = sig;
      return self;
   }

   auto Modal::with_remark( this auto&& self, Remark const& remark ) noexcept -> decltype( self )
   {
      if ( self.remarks_count_ < self.max_accessories_count_ )
      {
         self.remarks_[self.remarks_count_++] = remark;
      }
      return self;
   }

   auto Modal::with_choice( this auto&& self, Choice const& choice ) noexcept -> decltype( self )
   {
      if ( self.choices_count_ >= self.max_accessories_count_ || choice.label.empty( ) )
      {
         return self;
      }
      //
      self.choices_[self.choices_count_++] = choice;
      //
      switch ( choice.tag )
      {
         default:
         case Choice::Tag::none  : break;
         case Choice::Tag::master: self.master_choice_ = self.choices_count_ - 1U; break;
         case Choice::Tag::cancel: self.cancel_choice_ = self.choices_count_ - 1U; break;
      }
      //
      return self;
   }
}


#endif //!ASH_MONOLITH_HPP
