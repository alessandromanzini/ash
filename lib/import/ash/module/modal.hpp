#ifndef ASH_MODAL_HPP
#define ASH_MODAL_HPP

#include <ash/pch.hpp>

#include <ash/module/signal.hpp>
#include <ash/module/theme.hpp>


namespace ash
{
   /**
    * Styles for the modal title and descriptions.
    */
   namespace style
   {
      enum class Font : uint8_t { normal, bold, italic };
      enum class TextAlignment : uint8_t { left, center, right, justified };
   }

   /**
    * A \p Description represents a text body in the \p Modal view.
    */
   struct Description // TODO: Allow override for alignment
   {
      static constexpr float header_font_size = 16.0f;
      static constexpr float body_font_size = 14.0f;
      static constexpr float caption_font_size = 10.0f;

      std::string_view content;
      float font_size = body_font_size;
      style::Font font_style = style::Font::normal;
      style::TextAlignment alignment = style::TextAlignment::left;

      [[nodiscard]] static auto as_header( std::string_view const content ) noexcept -> Description
      {
         return { .content = content, .font_size = header_font_size };
      }

      [[nodiscard]] static auto as_header_bold( std::string_view const content ) noexcept -> Description
      {
         return { .content = content, .font_size = header_font_size, .font_style = style::Font::bold };
      }

      [[nodiscard]] static auto as_body( std::string_view const content ) noexcept -> Description
      {
         return { .content = content, .font_size = body_font_size };
      }

      [[nodiscard]] static auto as_caption( std::string_view const content ) noexcept -> Description
      {
         return { .content = content, .font_size = caption_font_size, .font_style = style::Font::italic };
      }
   };

   /**
    * A \p Choice represents a button selection in response to the \p Modal view.
    */
   struct Choice
   {
      std::string_view label;
      uint8_t token = UINT8_MAX;
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

   namespace detail
   {
      struct ModalRequest
      {
         std::string_view title;

         Signal signal;
         theme::Color signal_color;
         std::string_view signal_label;

         std::span<Description const> descriptions;
         std::span<Choice const> choices;

         size_t master_choice;
         bool has_cancel;
         size_t cancel_choice;

         float max_width;
         float min_width;
      };

      ASH_EXPORT [[nodiscard]] auto open_modal( ModalRequest const& request ) noexcept -> size_t;
   }
}


#endif //!ASH_MODAL_HPP
