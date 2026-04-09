#pragma once
#ifndef LOGGINATOR_FORMATOR_HPP_INCLUDED
#define LOGGINATOR_FORMATOR_HPP_INCLUDED

#include <charconv>
#include <concepts>
#include <span>
#include <string_view>

namespace logginator::formator
{
  enum class IntergerFormat
  {
    ascii,
    hex,
    b64,
    default_fmt = ascii,
  };

  enum class FloatFormat
  {
    ascii,
    ascii_fixed,
    ascii_scientific,
    hex,
    b64,
    default_fmt = ascii,
  };

  std::to_chars_result append_base64(char* pos, char* end, std::span<std::byte const> value);
  std::to_chars_result append_string(char* pos, char* end, std::string_view value);
  std::to_chars_result append_n_chars(char* pos, char* end, char c, std::size_t count);

  template <typename T>
    requires(std::integral<T>)
  std::to_chars_result append_int(char* pos, char* end, T const& value, IntergerFormat fmt)
  {
    switch (fmt)
    {
    case IntergerFormat::hex:
      return std::to_chars(pos, end, value, 16);

    case IntergerFormat::b64:
      return logginator::formator::append_base64(pos, end, std::span<std::byte const>(reinterpret_cast<std::byte const*>(&value), sizeof(value)));

    case IntergerFormat::ascii:
    default:
      return std::to_chars(pos, end, value, 10);
    }
  }

  template <typename T>
    requires(std::floating_point<T>)
  std::to_chars_result append_float(char* pos, char* end, T const& value, FloatFormat fmt)
  {
    switch (fmt)
    {
    case FloatFormat::ascii_fixed:
      return std::to_chars(pos, end, value, std::chars_format::fixed);
    case FloatFormat::ascii_scientific:
      return std::to_chars(pos, end, value, std::chars_format::scientific);
    case FloatFormat::hex:
      return std::to_chars(pos, end, value, std::chars_format::hex);
    case FloatFormat::b64:
      return logginator::formator::append_base64(pos, end, std::span<std::byte const>(reinterpret_cast<std::byte const*>(&value), sizeof(value)));
    default:
      return std::to_chars(pos, end, value, std::chars_format::general);
    }
  }

}    // namespace logginator::formator

#endif