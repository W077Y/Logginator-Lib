#include <logginator-printer.hpp>
//
#include <charconv>
#include <exception>
#include <string_view>
#include <system_error>

namespace
{
  std::to_chars_result append(char* pos, char* end, char c, std::size_t count);

  std::to_chars_result append(char* pos, char* end, std::string_view txt);

  std::to_chars_result append(char* pos, char* end, std::span<std::byte const> value)
  {
    constexpr char tab[64] = {
      'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',    //
      'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',    //
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'                                                                           //
    };

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
        *pos++ = tab[(tmp & 0xFC'00'00) >> 18];
        *pos++ = tab[(tmp & 0x03'F0'00) >> 12];
        *pos++ = tab[(tmp & 0x00'0F'C0) >> 6];
        *pos++ = tab[(tmp & 0x00'00'3F) >> 0];

        tmp = 0;
        sft = 24;
      }
    }

    if (sft / 8 == 2)
    {
      *pos++ = tab[(tmp & 0xFC'00'00) >> 18];
      *pos++ = tab[(tmp & 0x03'F0'00) >> 12];
      *pos++ = '=';
      *pos++ = '=';
    }

    if (sft / 8 == 1)
    {
      *pos++ = tab[(tmp & 0xFC'00'00) >> 18];
      *pos++ = tab[(tmp & 0x03'F0'00) >> 12];
      *pos++ = tab[(tmp & 0x00'0F'C0) >> 6];
      *pos++ = '=';
    }

    return { .ptr = pos, .ec = std::errc() };
  }

  std::to_chars_result append_channel_number(char* pos, char* end, uint8_t channel_number)
  {
    auto ret = append(pos, end, "#");
    if (ret.ec != std::errc())
    {
      return ret;
    }
    ret = std::to_chars(ret.ptr, end, (channel_number >> 4) & 0x0F, 16);
    if (ret.ec != std::errc())
    {
      return ret;
    }
    ret = std::to_chars(ret.ptr, end, (channel_number >> 0) & 0x0F, 16);
    if (ret.ec != std::errc())
    {
      return ret;
    }
    return { .ptr = ret.ptr, .ec = std::errc() };
  }

  std::to_chars_result append(char* pos, char* end, std::string_view txt)
  {
    for (char c : txt)
    {
      if (pos == end)
      {
        return { .ptr = end, .ec = std::errc::no_buffer_space };
      }
      *pos++ = c;
    }

    return { .ptr = pos, .ec = std::errc() };
  }

  std::to_chars_result append(char* pos, char* end, char c, std::size_t count)
  {
    for (std::size_t i = 0; i < count; i++)
    {
      if (pos >= end)
        return { .ptr = end, .ec = std::errc::no_buffer_space };

      *pos++ = c;
    }
    return { .ptr = pos, .ec = std::errc() };
  }

  std::to_chars_result append_column(char* pos, char* end, std::string_view content, std::size_t min_width)
  {
    std::size_t const len      = content.length();
    std::size_t       fill_cnt = 0;
    if (len < min_width)
    {
      fill_cnt += min_width - len;
    }

    auto ret = append(pos, end, ' ', fill_cnt);
    if (ret.ec != std::errc())
    {
      return ret;
    }

    ret = append(ret.ptr, end, content);
    if (ret.ec != std::errc())
    {
      return ret;
    }

    ret = append(ret.ptr, end, ";");
    if (ret.ec != std::errc())
    {
      return ret;
    }

    return ret;
  }

  std::to_chars_result append(char* pos, char* end, logginator::column_description_int::Format fmt)
  {
    switch (fmt)
    {
      using enum logginator::column_description_int::Format;
    case hex:
      return append(pos, end, "int_hex");
    case b64:
      return append(pos, end, "int_b64");

    case ascii:
    default:
      break;
    }
    return append(pos, end, "int_ascii");
  }

  std::to_chars_result append(char* pos, char* end, logginator::column_description_float::Format fmt)
  {
    switch (fmt)
    {
      using enum logginator::column_description_float::Format;
    case ascii_fixed:
      return append(pos, end, "float_fixed");
    case ascii_scientific:
      return append(pos, end, "float_scientific");
    case hex:
      return append(pos, end, "float_hex");
    case b64:
      return append(pos, end, "float_b64");

    case ascii:
    default:
      break;
    }
    return append(pos, end, "float_ascii");
  }

  std::to_chars_result append(char* pos, char* end, logginator::column_description_binary::Format fmt)
  {
    switch (fmt)
    {
      using enum logginator::column_description_binary::Format;
    case b64:
      return append(pos, end, "binary_b64");

    default:
      break;
    }
    return append(pos, end, "binary_b64");
  }

  std::to_chars_result append(char* pos, char* end, logginator::column_description_string::Format fmt)
  {
    switch (fmt)
    {
      using enum logginator::column_description_string::Format;
    case ascii:
      return append(pos, end, "string_ascii");

    default:
      break;
    }
    return append(pos, end, "string_ascii");
  }

  template <typename T> std::to_chars_result append_column_desciption(char* pos, char* end, T const& description)
  {
    auto ret = append(pos, end, description.get_name());
    if (ret.ec != std::errc())
    {
      return ret;
    }
    ret = append(ret.ptr, end, "[");
    if (ret.ec != std::errc())
    {
      return ret;
    }
    ret = append(ret.ptr, end, description.get_unit());
    if (ret.ec != std::errc())
    {
      return ret;
    }
    ret = append(ret.ptr, end, "]{");
    if (ret.ec != std::errc())
    {
      return ret;
    }
    ret = append(ret.ptr, end, description.get_format());
    if (ret.ec != std::errc())
    {
      return ret;
    }
    ret = append(ret.ptr, end, "};");
    if (ret.ec != std::errc())
    {
      return ret;
    }
    return ret;
  }

  std::to_chars_result append_int_ascii(char* pos, char* end, signed long long int const& value)
  {
    if (value >= 0)
    {
      *pos++ = ' ';
    }
    return std::to_chars(pos, end, value);
  }

  std::to_chars_result append_int_ascii(char* pos, char* end, unsigned long long int const& value) { return std::to_chars(pos, end, value); }

  std::to_chars_result append_int_hex(char* pos, char* end, signed long long int const& value) { return std::to_chars(pos, end, value, 16); }

  std::to_chars_result append_int_hex(char* pos, char* end, unsigned long long int const& value) { return std::to_chars(pos, end, value, 16); }

  std::to_chars_result append_int(char* pos, char* end, signed long long int const& value, logginator::column_description_int::Format fmt)
  {
    using Format = logginator::column_description_int::Format;
    switch (fmt)
    {
    case Format::hex:
      return append_int_hex(pos, end, value);

    case Format::b64:
      return append(pos, end, std::span<std::byte const>(reinterpret_cast<std::byte const*>(&value), sizeof(value)));

    case Format::ascii:
    default:
      break;
    }

    return append_int_ascii(pos, end, value);
  }

  std::to_chars_result append_int(char* pos, char* end, unsigned long long int const& value, logginator::column_description_int::Format fmt)
  {
    using Format = logginator::column_description_int::Format;
    switch (fmt)
    {
    case Format::hex:
      return append_int_hex(pos, end, value);

    case Format::b64:
      return append(pos, end, std::span<std::byte const>(reinterpret_cast<std::byte const*>(&value), sizeof(value)));

    case Format::ascii:
    default:
      break;
    }

    return append_int_ascii(pos, end, value);
  }

  std::to_chars_result append_float(char* pos, char* end, long double const& value, std::chars_format fmt) { return std::to_chars(pos, end, value, fmt); }

  std::to_chars_result append_float(char* pos, char* end, long double const& value, logginator::column_description_float::Format fmt)
  {
    using Format = logginator::column_description_float::Format;
    if (value >= 0.0)
    {
      *pos++ = ' ';
    }
    switch (fmt)
    {
    case Format::ascii_fixed:
      return append_float(pos, end, value, std::chars_format::fixed);

    case Format::ascii_scientific:
      return append_float(pos, end, value, std::chars_format::scientific);

    case Format::hex:
      return append_float(pos, end, value, std::chars_format::hex);

    case Format::b64:
      return append(pos, end, std::span<std::byte const>(reinterpret_cast<std::byte const*>(&value), sizeof(value)));

    case Format::ascii:
    default:
      break;
    }

    return append_float(pos, end, value, std::chars_format::general);
  }

}    // namespace

namespace logginator
{
  Printer_Base::Printer_Base(Manager_Interface& man, std::span<char> buffer, channel_description_t cfg, print_header_fnc fnc)
      : m_man{ man }
      , m_default_msg{ fnc }
      , m_cfg{ cfg }
      , m_buffer_begin{ buffer.data() }
      , m_buffer_end{ buffer.data() + buffer.size() }
  {
    if (buffer.size() < 6)
    {
      throw std::exception();
    }
  }

  auto Printer_Base::request_line() -> line_t { return this->request_line(false); }

  line_t Printer_Base::request_line(bool is_header)
  {
    this->lock_line();
    if (this->m_pos != nullptr)
    {
      throw std::exception();
    }

    this->m_pos = this->m_buffer_begin;
    auto ret    = append_channel_number(this->m_buffer_begin, this->m_buffer_end, this->m_cfg.ID);
    if (is_header)
    {
      ret = append(ret.ptr, this->m_buffer_end, ":");
      if (ret.ec != std::errc())
      {
        throw std::exception();
      }

      ret = append(ret.ptr, this->m_buffer_end, this->m_cfg.name);
      if (ret.ec != std::errc())
      {
        throw std::exception();
      }
    }

    ret = append(ret.ptr, this->m_buffer_end, ";");
    if (ret.ec != std::errc())
    {
      throw std::exception();
    }
    this->m_pos = ret.ptr;

    return line_t{ *this, is_header };
  }

  channel_description_t const& Printer_Base::get_cfg() const { return this->m_cfg; }

  void Printer_Base::setup(uint32_t downsample_factor)
  {
    this->m_downsample_trg = downsample_factor;
    this->m_downsample_cnt = downsample_factor;
  }

  void Printer_Base::print_header()
  {
    auto line = this->request_line(true);
    this->m_default_msg(line);
  }

  void Printer_Base::publish(bool is_header)
  {
    if (this->m_pos > this->m_buffer_begin)
    {
      *--this->m_pos = '\n';
      *++this->m_pos = '\0';
    }

    std::string_view msg{ this->m_buffer_begin, this->m_pos };
    this->m_pos = nullptr;

    if (is_header)
    {
      this->m_man.publish(msg);
      this->unlock_line();
      return;
    }

    if (this->m_downsample_trg == 0)
    {
      this->unlock_line();
      return;
    }

    if (++this->m_downsample_cnt >= this->m_downsample_trg)
    {
      this->m_downsample_cnt = 0;
    }

    if (this->m_downsample_cnt != 0)
    {
      this->unlock_line();
      return;
    }

    this->m_man.publish(this->m_buffer_begin);
    this->unlock_line();
  }

  void Printer_Base::add(bool const& value, column_description_int::Format format) { return this->add(static_cast<unsigned long long int>(value), format); }

  void Printer_Base::add(char const& value, column_description_int::Format format) { return this->add(static_cast<signed long long int>(value), format); }

  void Printer_Base::add(signed char const& value, column_description_int::Format format)
  {
    return this->add(static_cast<signed long long int>(value), format);
  }

  void Printer_Base::add(unsigned char const& value, column_description_int::Format format)
  {
    return this->add(static_cast<unsigned long long int>(value), format);
  }

  void Printer_Base::add(char8_t const& value, column_description_int::Format format) { return this->add(static_cast<unsigned long long int>(value), format); }

  void Printer_Base::add(char16_t const& value, column_description_int::Format format) { return this->add(static_cast<unsigned long long int>(value), format); }

  void Printer_Base::add(char32_t const& value, column_description_int::Format format) { return this->add(static_cast<unsigned long long int>(value), format); }

  void Printer_Base::add(signed short const& value, column_description_int::Format format)
  {
    return this->add(static_cast<signed long long int>(value), format);
  }

  void Printer_Base::add(unsigned short const& value, column_description_int::Format format)
  {
    return this->add(static_cast<unsigned long long int>(value), format);
  }

  void Printer_Base::add(signed int const& value, column_description_int::Format format) { return this->add(static_cast<signed long long int>(value), format); }

  void Printer_Base::add(unsigned int const& value, column_description_int::Format format)
  {
    return this->add(static_cast<unsigned long long int>(value), format);
  }

  void Printer_Base::add(signed long int const& value, column_description_int::Format format)
  {
    return this->add(static_cast<signed long long int>(value), format);
  }

  void Printer_Base::add(unsigned long int const& value, column_description_int::Format format)
  {
    return this->add(static_cast<unsigned long long int>(value), format);
  }

  void Printer_Base::add(signed long long int const& value, column_description_int::Format format)
  {
    char tmp[50] = {};
    auto ret     = append_int(tmp, tmp + sizeof(tmp), value, format);
    if (ret.ec != std::errc())
    {
      throw std::exception();
    }

    constexpr std::size_t column_width = 20;
    ret                                = append_column(this->m_pos, this->m_buffer_end, std::string_view{ tmp, ret.ptr }, column_width);
    if (ret.ec != std::errc())
    {
      throw std::exception();
    }

    this->m_pos = ret.ptr;
  }

  void Printer_Base::add(unsigned long long int const& value, column_description_int::Format format)
  {
    char tmp[50] = {};
    auto ret     = append_int(tmp, tmp + sizeof(tmp), value, format);
    if (ret.ec != std::errc())
    {
      throw std::exception();
    }

    constexpr std::size_t column_width = 20;
    ret                                = append_column(this->m_pos, this->m_buffer_end, std::string_view{ tmp, ret.ptr }, column_width);
    if (ret.ec != std::errc())
    {
      throw std::exception();
    }

    this->m_pos = ret.ptr;
  }

  void Printer_Base::add(float const& value, column_description_float::Format format) { return this->add(static_cast<long double>(value), format); }

  void Printer_Base::add(double const& value, column_description_float::Format format) { return this->add(static_cast<long double>(value), format); }

  void Printer_Base::add(long double const& value, column_description_float::Format format)
  {
    char tmp[50] = {};

    auto ret = append_float(tmp, tmp + sizeof(tmp), value, format);
    if (ret.ec != std::errc())
    {
      throw std::exception();
    }

    constexpr std::size_t column_width = 25;
    ret                                = append_column(this->m_pos, this->m_buffer_end, std::string_view{ tmp, ret.ptr }, column_width);
    if (ret.ec != std::errc())
    {
      throw std::exception();
    }

    this->m_pos = ret.ptr;
  }

  void Printer_Base::add(std::byte const& value, column_description_binary::Format format)
  {
    return this->add(std::span<std::byte const>{ &value, 1 }, format);
  }

  void Printer_Base::add(std::span<std::byte const> value, column_description_binary::Format)
  {
    auto ret = append(this->m_pos, this->m_buffer_end, value);
    if (ret.ec != std::errc())
    {
      throw std::exception();
    }

    ret = append(ret.ptr, this->m_buffer_end, ";");
    if (ret.ec != std::errc())
    {
      throw std::exception();
    }

    this->m_pos = ret.ptr;
  }

  void Printer_Base::add(std::string_view value, column_description_string::Format)
  {
    auto ret = append_column(this->m_pos, this->m_buffer_end, value, 0);
    if (ret.ec != std::errc())
    {
      throw std::exception();
    }
    this->m_pos = ret.ptr;
  }

  void Printer_Base::add(column_description_int const& description)
  {
    auto ret = append_column_desciption(this->m_pos, this->m_buffer_end, description);
    if (ret.ec != std::errc())
    {
      throw std::exception();
    }
    this->m_pos = ret.ptr;
  }

  void Printer_Base::add(column_description_float const& description)
  {
    auto ret = append_column_desciption(this->m_pos, this->m_buffer_end, description);
    if (ret.ec != std::errc())
    {
      throw std::exception();
    }
    this->m_pos = ret.ptr;
  }

  void Printer_Base::add(column_description_binary const& description)
  {
    auto ret = append_column_desciption(this->m_pos, this->m_buffer_end, description);
    if (ret.ec != std::errc())
    {
      throw std::exception();
    }
    this->m_pos = ret.ptr;
  }

  void Printer_Base::add(column_description_string const& description)
  {
    auto ret = append_column_desciption(this->m_pos, this->m_buffer_end, description);
    if (ret.ec != std::errc())
    {
      throw std::exception();
    }
    this->m_pos = ret.ptr;
  }
}    // namespace logginator
