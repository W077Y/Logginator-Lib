#include <logginator-printer.hpp>
//
#include <charconv>
#include <exception>
#include <logginator-formator.hpp>
#include <string_view>
#include <system_error>

namespace
{
  std::to_chars_result append_channel_number(char* pos, char* end, uint8_t channel_number)
  {
    auto ret = logginator::formator::append_string(pos, end, "#");
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

  // std::to_chars_result append_column(char* pos, char* end, std::string_view content, std::size_t min_width)
  // {
  //   std::size_t const len      = content.length();
  //   std::size_t       fill_cnt = 0;
  //   if (len < min_width)
  //   {
  //     fill_cnt += min_width - len;
  //   }

  //   auto ret = logginator::formator::append_n_chars(pos, end, ' ', fill_cnt);
  //   if (ret.ec != std::errc())
  //   {
  //     return ret;
  //   }

  //   ret = logginator::formator::append_string(ret.ptr, end, content);
  //   if (ret.ec != std::errc())
  //   {
  //     return ret;
  //   }

  //   ret = logginator::formator::append_string(ret.ptr, end, ";");
  //   if (ret.ec != std::errc())
  //   {
  //     return ret;
  //   }

  //   return ret;
  // }

  // std::to_chars_result append(char* pos, char* end, logginator::column_description_int::Format fmt)
  // {
  //   switch (fmt)
  //   {
  //     using enum logginator::column_description_int::Format;
  //   case hex:
  //     return logginator::formator::append_string(pos, end, "int_hex");
  //   case b64:
  //     return logginator::formator::append_string(pos, end, "int_b64");

  //   case ascii:
  //   default:
  //     break;
  //   }
  //   return logginator::formator::append_string(pos, end, "int_ascii");
  // }

  // std::to_chars_result append(char* pos, char* end, logginator::column_description_float::Format fmt)
  // {
  //   switch (fmt)
  //   {
  //     using enum logginator::column_description_float::Format;
  //   case ascii_fixed:
  //     return logginator::formator::append_string(pos, end, "float_fixed");
  //   case ascii_scientific:
  //     return logginator::formator::append_string(pos, end, "float_scientific");
  //   case hex:
  //     return logginator::formator::append_string(pos, end, "float_hex");
  //   case b64:
  //     return logginator::formator::append_string(pos, end, "float_b64");

  //   case ascii:
  //   default:
  //     break;
  //   }
  //   return logginator::formator::append_string(pos, end, "float_ascii");
  // }

  // std::to_chars_result append(char* pos, char* end, logginator::column_description_binary::Format fmt)
  // {
  //   switch (fmt)
  //   {
  //     using enum logginator::column_description_binary::Format;
  //   case b64:
  //     return logginator::formator::append_string(pos, end, "binary_b64");

  //   default:
  //     break;
  //   }
  //   return logginator::formator::append_string(pos, end, "binary_b64");
  // }

  // std::to_chars_result append(char* pos, char* end, logginator::column_description_string::Format fmt)
  // {
  //   switch (fmt)
  //   {
  //     using enum logginator::column_description_string::Format;
  //   case ascii:
  //     return logginator::formator::append_string(pos, end, "string_ascii");

  //   default:
  //     break;
  //   }
  //   return logginator::formator::append_string(pos, end, "string_ascii");
  // }

  template <typename T> std::to_chars_result append_column_desciption(char* pos, char* end, T const& description)
  {
    auto ret = logginator::formator::append_string(pos, end, description.get_name());
    if (ret.ec != std::errc())
    {
      return ret;
    }
    ret = logginator::formator::append_string(ret.ptr, end, "[");
    if (ret.ec != std::errc())
    {
      return ret;
    }
    ret = logginator::formator::append_string(ret.ptr, end, description.get_unit());
    if (ret.ec != std::errc())
    {
      return ret;
    }
    ret = logginator::formator::append_string(ret.ptr, end, "]{");
    if (ret.ec != std::errc())
    {
      return ret;
    }
    ret = append(ret.ptr, end, description.get_format());
    if (ret.ec != std::errc())
    {
      return ret;
    }
    ret = logginator::formator::append_string(ret.ptr, end, "};");
    if (ret.ec != std::errc())
    {
      return ret;
    }
    return ret;
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
    this->m_pos = this->m_buffer_begin;
    auto ret    = append_channel_number(this->m_buffer_begin, this->m_buffer_end, this->m_cfg.ID);
    if (is_header)
    {
      ret = logginator::formator::append_string(ret.ptr, this->m_buffer_end, ":");
      if (ret.ec != std::errc())
      {
        throw std::exception();
      }

      ret = logginator::formator::append_string(ret.ptr, this->m_buffer_end, this->m_cfg.name);
      if (ret.ec != std::errc())
      {
        throw std::exception();
      }
    }

    ret = logginator::formator::append_string(ret.ptr, this->m_buffer_end, ";");
    if (ret.ec != std::errc())
    {
      throw std::exception();
    }
    this->m_pos = ret.ptr;

    return line_t{ *this, this->m_buffer_begin, this->m_pos, this->m_buffer_end, is_header };
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

  void Printer_Base::publish(bool is_header, std::string_view msg)
  {
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

  // void Printer_Base::add(signed long long int const& value, column_description_int::Format format)
  // {
  //   char tmp[50] = {};
  //   auto ret     = formator::append_int(tmp, tmp + sizeof(tmp), value, format);
  //   if (ret.ec != std::errc())
  //   {
  //     throw std::exception();
  //   }

  //   constexpr std::size_t column_width = 20;
  //   ret                                = append_column(this->m_pos, this->m_buffer_end, std::string_view{ tmp, ret.ptr }, column_width);
  //   if (ret.ec != std::errc())
  //   {
  //     throw std::exception();
  //   }

  //   this->m_pos = ret.ptr;
  // }

  // void Printer_Base::add(unsigned long long int const& value, column_description_int::Format format)
  // {
  //   char tmp[50] = {};
  //   auto ret     = append_int(tmp, tmp + sizeof(tmp), value, format);
  //   if (ret.ec != std::errc())
  //   {
  //     throw std::exception();
  //   }

  //   constexpr std::size_t column_width = 20;
  //   ret                                = append_column(this->m_pos, this->m_buffer_end, std::string_view{ tmp, ret.ptr }, column_width);
  //   if (ret.ec != std::errc())
  //   {
  //     throw std::exception();
  //   }

  //   this->m_pos = ret.ptr;
  // }

  // void Printer_Base::add(float const& value, column_description_float::Format format) { return this->add(static_cast<long double>(value), format); }

  // void Printer_Base::add(double const& value, column_description_float::Format format) { return this->add(static_cast<long double>(value), format); }

  // void Printer_Base::add(long double const& value, column_description_float::Format format)
  // {
  //   char tmp[50] = {};

  //   auto ret = append_float(tmp, tmp + sizeof(tmp), value, format);
  //   if (ret.ec != std::errc())
  //   {
  //     throw std::exception();
  //   }

  //   constexpr std::size_t column_width = 25;
  //   ret                                = append_column(this->m_pos, this->m_buffer_end, std::string_view{ tmp, ret.ptr }, column_width);
  //   if (ret.ec != std::errc())
  //   {
  //     throw std::exception();
  //   }

  //   this->m_pos = ret.ptr;
  // }

  // void Printer_Base::add(std::byte const& value, column_description_binary::Format format)
  // {
  //   return this->add(std::span<std::byte const>{ &value, 1 }, format);
  // }

  // void Printer_Base::add(std::span<std::byte const> value, column_description_binary::Format)
  // {
  //   auto ret = logginator::formator::append_base64(this->m_pos, this->m_buffer_end, value);
  //   if (ret.ec != std::errc())
  //   {
  //     throw std::exception();
  //   }

  //   ret = logginator::formator::append_string(ret.ptr, this->m_buffer_end, ";");
  //   if (ret.ec != std::errc())
  //   {
  //     throw std::exception();
  //   }

  //   this->m_pos = ret.ptr;
  // }

  // void Printer_Base::add(std::string_view value, column_description_string::Format)
  // {
  //   auto ret = append_column(this->m_pos, this->m_buffer_end, value, 0);
  //   if (ret.ec != std::errc())
  //   {
  //     throw std::exception();
  //   }
  //   this->m_pos = ret.ptr;
  // }

  // void Printer_Base::add(column_description_int const& description)
  // {
  //   auto ret = append_column_desciption(this->m_pos, this->m_buffer_end, description);
  //   if (ret.ec != std::errc())
  //   {
  //     throw std::exception();
  //   }
  //   this->m_pos = ret.ptr;
  // }

  // void Printer_Base::add(column_description_float const& description)
  // {
  //   auto ret = append_column_desciption(this->m_pos, this->m_buffer_end, description);
  //   if (ret.ec != std::errc())
  //   {
  //     throw std::exception();
  //   }
  //   this->m_pos = ret.ptr;
  // }

  // void Printer_Base::add(column_description_binary const& description)
  // {
  //   auto ret = append_column_desciption(this->m_pos, this->m_buffer_end, description);
  //   if (ret.ec != std::errc())
  //   {
  //     throw std::exception();
  //   }
  //   this->m_pos = ret.ptr;
  // }

  // void Printer_Base::add(column_description_string const& description)
  // {
  //   auto ret = append_column_desciption(this->m_pos, this->m_buffer_end, description);
  //   if (ret.ec != std::errc())
  //   {
  //     throw std::exception();
  //   }
  //   this->m_pos = ret.ptr;
  // }
}    // namespace logginator
