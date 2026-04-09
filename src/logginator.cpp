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
  }    // namespace

  line_t::line_t(Printer_Interface& printer, char* begin, char* pos, char* end, bool print_header)

      : m_printer(printer)
      , m_header(print_header)
      , m_begin(begin)
      , m_pos(pos)
      , m_end(end)
  {
  }

  line_t::~line_t()
  {
    if (this->m_pos > this->m_begin)
    {
      *--this->m_pos = '\n';
      *++this->m_pos = '\0';
    }
    this->m_printer.publish(this->m_header, std::string_view{ this->m_begin, this->m_pos });
  }

  bool line_t::add(std::string_view name, std::string_view unit, std::string_view format)
  {
    auto ret = logginator::formator::append_string(this->m_pos, this->m_end, name);
    if (ret.ec != std::errc())
    {
      return false;
    }

    ret = logginator::formator::append_string(ret.ptr, this->m_end, "[");
    if (ret.ec != std::errc())
    {
      return false;
    }

    ret = logginator::formator::append_string(ret.ptr, this->m_end, unit);
    if (ret.ec != std::errc())
    {
      return false;
    }

    ret = logginator::formator::append_string(ret.ptr, this->m_end, "]{");
    if (ret.ec != std::errc())
    {
      return false;
    }

    ret = logginator::formator::append_string(ret.ptr, this->m_end, format);
    if (ret.ec != std::errc())
    {
      return false;
    }

    ret = logginator::formator::append_string(ret.ptr, this->m_end, "};");
    if (ret.ec != std::errc())
    {
      return false;
    }

    this->m_pos = ret.ptr;
    return true;
  }

  bool line_t::add(column_description_int description)
  {
    return this->add(description.get_name(), description.get_unit(), to_string_view(description.get_format()));
  }

  bool line_t::add(column_description_float description)
  {
    return this->add(description.get_name(), description.get_unit(), to_string_view(description.get_format()));
  }

  bool line_t::add(column_description_binary description)
  {
    return this->add(description.get_name(), description.get_unit(), to_string_view(description.get_format()));
  }

  bool line_t::add(column_description_string description)
  {
    return this->add(description.get_name(), description.get_unit(), to_string_view(description.get_format()));
  }
}    // namespace logginator
