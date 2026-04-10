#include <logginator.hpp>
#include <string_view>

namespace logginator
{
  namespace
  {
    constexpr std::string_view to_string_view(logginator::column_description_int::Format fmt)
    {
      switch (fmt)
      {
        using enum logginator::column_description_int::Format;
      case hex:
        return "int_hex";
      case b64:
        return "int_b64";

      case ascii:
      default:
        break;
      }
      return "int_ascii";
    }

    constexpr std::string_view to_string_view(logginator::column_description_float::Format fmt)
    {
      switch (fmt)
      {
        using enum logginator::column_description_float::Format;
      case ascii_fixed:
        return "float_fixed";
      case ascii_scientific:
        return "float_scientific";
      case hex:
        return "float_hex";
      case b64:
        return "float_b64";

      case ascii:
      default:
        break;
      }
      return "float_ascii";
    }

    constexpr std::string_view to_string_view(logginator::column_description_binary::Format fmt)
    {
      switch (fmt)
      {
        using enum logginator::column_description_binary::Format;
      case b64:
        return "binary_b64";

      default:
        break;
      }
      return "binary_b64";
    }

    constexpr std::string_view to_string_view(logginator::column_description_string::Format fmt)
    {
      switch (fmt)
      {
        using enum logginator::column_description_string::Format;
      case ascii:
        return "string_ascii";

      default:
        break;
      }
      return "string_ascii";
    }

    constexpr std::to_chars_result append_channel_number(char* pos, char* end, uint8_t channel_number, std::string_view channel_name)
    {
      auto ret = formator::append_string(pos, end, "#");
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
      if (!channel_name.empty())
      {
        ret = formator::append_string(ret.ptr, end, ":");
        if (ret.ec != std::errc())
        {
          return ret;
        }
        ret = formator::append_string(ret.ptr, end, channel_name);
        if (ret.ec != std::errc())
        {
          return ret;
        }
      }
      ret = logginator::formator::append_string(ret.ptr, end, ";");
      if (ret.ec != std::errc())
      {
        return ret;
      }
      return { .ptr = ret.ptr, .ec = std::errc() };
    }
  }    // namespace

  line_t::line_t(Channel_Interface& channel, std::span<char> buffer, bool print_header)

      : m_channel(channel)
      , m_header(print_header)
      , m_begin(buffer.data())
      , m_pos(this->m_begin)
      , m_end(buffer.data() + buffer.size())
  {
    auto ret = append_channel_number(this->m_begin, this->m_end,              //
                                     this->m_channel.channel.get_cfg().ID,    //
                                     this->m_header ? this->m_channel.channel.get_cfg().name : std::string_view{});
    if (ret.ec != std::errc())
    {
      throw logginator::errors::line_serialization_error();
    }
    this->m_pos = ret.ptr;
  }

  line_t::~line_t()
  {
    if (this->m_pos > this->m_begin)
    {
      *(this->m_pos - 1) = '\n';
    }

    auto msg = std::string_view{ this->m_begin, this->m_pos };
    if (msg.length() < 5)    // "#XX\n" empty message, don't publish
    {
      return;
    }

    this->m_channel.channel.publish(this->m_header, msg);
  }

  void line_t::add(std::string_view name, std::string_view unit, std::string_view format)
  {
    auto ret = logginator::formator::append_string(this->m_pos, this->m_end, name);
    if (ret.ec != std::errc())
    {
      throw logginator::errors::line_serialization_error();
    }

    ret = logginator::formator::append_string(ret.ptr, this->m_end, "[");
    if (ret.ec != std::errc())
    {
      throw logginator::errors::line_serialization_error();
    }

    ret = logginator::formator::append_string(ret.ptr, this->m_end, unit);
    if (ret.ec != std::errc())
    {
      throw logginator::errors::line_serialization_error();
    }

    ret = logginator::formator::append_string(ret.ptr, this->m_end, "]{");
    if (ret.ec != std::errc())
    {
      throw logginator::errors::line_serialization_error();
    }

    ret = logginator::formator::append_string(ret.ptr, this->m_end, format);
    if (ret.ec != std::errc())
    {
      throw logginator::errors::line_serialization_error();
    }

    ret = logginator::formator::append_string(ret.ptr, this->m_end, "};");
    if (ret.ec != std::errc())
    {
      throw logginator::errors::line_serialization_error();
    }

    this->m_pos = ret.ptr;
  }

  void line_t::add(column_description_int description)
  {
    return this->add(description.get_name(), description.get_unit(), to_string_view(description.get_format()));
  }

  void line_t::add(column_description_float description)
  {
    return this->add(description.get_name(), description.get_unit(), to_string_view(description.get_format()));
  }

  void line_t::add(column_description_binary description)
  {
    return this->add(description.get_name(), description.get_unit(), to_string_view(description.get_format()));
  }

  void line_t::add(column_description_string description)
  {
    return this->add(description.get_name(), description.get_unit(), to_string_view(description.get_format()));
  }
}    // namespace logginator
