#include "logginator-format.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace logginator::format
{
  namespace
  {
    constexpr char base_64_table[64] = {
      'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',    //
      'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',    //
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'                                                                           //
    };
  }

  std::to_chars_result append_base64(char* pos, char* end, std::span<std::byte const> value)
  {
    if (value.size() == 0)
    {
      return { .ptr = pos, .ec = std::errc() };
    }

    std::size_t const max_number_of_blk = (end - pos) / 4;
    if (max_number_of_blk < (value.size() / 3 + 1))
    {
      return { .ptr = end, .ec = std::errc::no_buffer_space };
    }

    uint32_t    tmp = 0;
    std::size_t sft = 24;
    for (std::byte b : value)
    {
      sft -= 8;
      tmp |= static_cast<uint32_t>(b) << sft;

      while (sft == 0)
      {
        *pos++ = base_64_table[(tmp & 0xFC'00'00) >> 18];
        *pos++ = base_64_table[(tmp & 0x03'F0'00) >> 12];
        *pos++ = base_64_table[(tmp & 0x00'0F'C0) >> 6];
        *pos++ = base_64_table[(tmp & 0x00'00'3F) >> 0];

        tmp = 0;
        sft = 24;
      }
    }

    if (sft == 16)
    {
      *pos++ = base_64_table[(tmp & 0xFC'00'00) >> 18];
      *pos++ = base_64_table[(tmp & 0x03'F0'00) >> 12];
      *pos++ = '=';
      *pos++ = '=';
    }

    if (sft == 8)
    {
      *pos++ = base_64_table[(tmp & 0xFC'00'00) >> 18];
      *pos++ = base_64_table[(tmp & 0x03'F0'00) >> 12];
      *pos++ = base_64_table[(tmp & 0x00'0F'C0) >> 6];
      *pos++ = '=';
    }

    return { .ptr = pos, .ec = std::errc() };
  }

  std::to_chars_result append_string(char* pos, char* end, std::string_view value)
  {
    if (value.empty())
    {
      return { .ptr = pos, .ec = std::errc() };
    }

    if (static_cast<std::size_t>(end - pos) < value.size())
    {
      return { .ptr = end, .ec = std::errc::no_buffer_space };
    }

    auto res = std::copy(value.begin(), value.end(), pos);
    return { .ptr = res, .ec = std::errc() };
  }

  std::to_chars_result append_n_chars(char* pos, char* end, char c, std::size_t count)
  {
    if (static_cast<std::size_t>(end - pos) < count)
    {
      return { .ptr = end, .ec = std::errc::no_buffer_space };
    }

    std::fill_n(pos, count, c);
    return { .ptr = pos + count, .ec = std::errc() };
  }

  std::to_chars_result append(char* pos, char* end, std::span<std::byte const> value, BinaryFormat fmt)
  {
    switch (fmt)
    {
    case BinaryFormat::b64:
      return format::append_base64(pos, end, value);
    default:
      return format::append_base64(pos, end, value);
    }
  }

  std::to_chars_result append(char* pos, char* end, std::string_view value, StringFormat fmt)
  {
    switch (fmt)
    {
    case StringFormat::b64:
      return format::append_base64(pos, end, std::span<std::byte const>(reinterpret_cast<std::byte const*>(value.data()), value.size()));
    case StringFormat::ascii:
    default:
      return format::append_string(pos, end, value);
    }
  }

}    // namespace logginator::format
