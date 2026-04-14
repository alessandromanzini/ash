#ifndef ASH_MONOLITH_HPP
#define ASH_MONOLITH_HPP

#include "ash/pch.hpp"

#include "ash/module/signal.hpp"


namespace ash
{
   // ───[[ STYLES ]]──────────────────────────────────────────────────────────────
   namespace style
   {
      enum class Font : uint8_t { normal, bold, italic };
      enum class TextAlignment : uint8_t { left, center, right, justified };
   }

   // ───[[ INSCRIPTION ]]─────────────────────────────────────────────────────────
   struct Inscription // TODO: Allow override for alignment
   {
      static constexpr float header_font_size  = 16.0f;
      static constexpr float body_font_size    = 14.0f;
      static constexpr float caption_font_size = 10.0f;

      std::string_view     content;
      float                font_size  = body_font_size;
      style::Font          font_style = style::Font::normal;
      style::TextAlignment alignment  = style::TextAlignment::left;

      [[nodiscard]] static auto as_header( std::string_view const content ) noexcept -> Inscription
      {
         return { .content = content, .font_size = header_font_size };
      }

      [[nodiscard]] static auto as_header_bold( std::string_view const content ) noexcept -> Inscription
      {
         return { .content = content, .font_size = header_font_size, .font_style = style::Font::bold };
      }

      [[nodiscard]] static auto as_body( std::string_view const content ) noexcept -> Inscription
      {
         return { .content = content, .font_size = body_font_size };
      }

      [[nodiscard]] static auto as_caption( std::string_view const content ) noexcept -> Inscription
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

   // ───[[ MONOLITH ]]────────────────────────────────────────────────────────────
   class Monolith final
   {
   public:
      explicit Monolith( std::string_view title ) noexcept;
      ~Monolith( ) noexcept = default;

      Monolith( Monolith const& )                        = delete;
      Monolith( Monolith&& ) noexcept                    = default;
      auto operator=( Monolith const& ) -> Monolith&     = delete;
      auto operator=( Monolith&& ) noexcept -> Monolith& = delete;

      [[nodiscard]] auto set_minimum_width( this auto&& self, float width ) noexcept -> decltype( self );
      [[nodiscard]] auto set_signal( this auto&& self, Signal sig ) noexcept -> decltype( self );

      [[nodiscard]] auto with_inscription( this auto&& self, Inscription const& inscription ) noexcept -> decltype( self );
      [[nodiscard]] auto with_choice( this auto&& self, Choice const& choice ) noexcept -> decltype( self );

      auto manifest( ) noexcept -> Choice;

   private:
      static constexpr float max_modal_width_ = 900.0f;

      std::string_view const title_;

      std::optional<float> min_width_;
      Signal               signal_ = Signal::info;

      static constexpr size_t                         max_accessories_count_ = 6U;
      std::array<Inscription, max_accessories_count_> inscriptions_{}; // use inplace_vector
      std::array<Choice, max_accessories_count_>      choices_{};

      uint8_t                inscriptions_count_ = 0U;
      uint8_t                choices_count_      = 0U;
      std::optional<uint8_t> master_choice_;
      std::optional<uint8_t> cancel_choice_;
   };

   inline Monolith::Monolith( std::string_view const title ) noexcept
      : title_{ title }
   { }

   auto Monolith::set_minimum_width( this auto&& self, float width ) noexcept -> decltype( self )
   {
      self.min_width_ = width;
      return self;
   }

   auto Monolith::set_signal( this auto&& self, Signal sig ) noexcept -> decltype( self )
   {
      self.signal_ = sig;
      return self;
   }

   auto Monolith::with_inscription( this auto&& self, Inscription const& inscription ) noexcept -> decltype( self )
   {
      if ( self.inscriptions_count_ < self.max_accessories_count_ )
      {
         self.inscriptions_[self.inscriptions_count_++] = inscription;
      }
      return self;
   }

   auto Monolith::with_choice( this auto&& self, Choice const& choice ) noexcept -> decltype( self )
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
