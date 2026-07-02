module;

#include <ash/pch.hpp>

#include <ash/module/modal.hpp>

export module ash:modal;

import :signal;
import :theme;


export namespace ash
{
   namespace style
   {
      using style::Font;
      using style::TextAlignment;
   }

   using ash::Choice;
   using ash::Description;

   // ───[[ MODAL ]]───────────────────────────────────────────────────────────────
   class Modal final
   {
   public:
      explicit Modal( std::string_view title ) noexcept
         : title_{ title }
      { }
      ~Modal( ) noexcept = default;

      Modal( Modal const& ) = delete;
      Modal( Modal&& ) noexcept = default;
      auto operator=( Modal const& ) -> Modal& = delete;
      auto operator=( Modal&& ) noexcept -> Modal& = delete;

      [[nodiscard]] auto set_minimum_width( this auto&& self, float width ) noexcept -> decltype( self )
      {
         self.min_width_ = width;
         return self;
      }

      [[nodiscard]] auto set_signal( this auto&& self, Signal sig ) noexcept -> decltype( self )
      {
         self.signal_ = sig;
         return self;
      }

      [[nodiscard]] auto with_desc( this auto&& self, Description const& desc ) noexcept -> decltype( self )
      {
         if ( self.descriptions_count_ < self.max_accessories_count_ )
         {
            self.descriptions_[self.descriptions_count_++] = desc;
         }
         return self;
      }

      [[nodiscard]] auto with_choice( this auto&& self, Choice const& choice ) noexcept -> decltype( self )
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

      auto open( ) noexcept -> Choice
      {
         size_t const selected = detail::open_modal(
           {
              .title = title_,
              .signal = signal_,
              .signal_color = theme::signal_to_color( signal_ ),
              .signal_label = reflection::to_string( signal_ ),
              .descriptions = { descriptions_.data( ), descriptions_count_ },
              .choices = {      choices_.data( ),      choices_count_ },
              .master_choice = master_choice_.value_or( 0 ),
              .has_cancel = cancel_choice_.has_value( ),
              .cancel_choice = cancel_choice_.value_or( 0 ),
              .max_width = max_modal_width_,
              .min_width = min_width_.value_or( 0.0f )
         } );
         return selected < choices_count_ ? choices_[selected] : Choice{};
      }

   private:
      static constexpr float max_modal_width_ = 900.0f;

      std::string_view const title_;

      std::optional<float> min_width_;
      Signal signal_ = Signal::info;

      static constexpr size_t max_accessories_count_ = 6U;
      std::array<Description, max_accessories_count_> descriptions_{}; // use inplace_vector
      std::array<Choice, max_accessories_count_> choices_{};

      uint8_t descriptions_count_ = 0U;
      uint8_t choices_count_ = 0U;
      std::optional<uint8_t> master_choice_;
      std::optional<uint8_t> cancel_choice_;
   };
}
