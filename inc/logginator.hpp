#pragma once
#ifndef LOGGINATOR_HPP_INCLUDED
#define LOGGINATOR_HPP_INCLUDED

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <logginator-formator.hpp>
#include <span>
#include <string_view>
#include <type_traits>

namespace logginator
{
  class Manager_Interface;
  class Printer_Interface;
  class line_t;

  struct channel_description_t
  {
    consteval channel_description_t(uint8_t id, char const* name)
        : ID{ id }
        , name{ name }
    {
    }

    uint8_t     ID;
    char const* name;
  };

  class column_description_t
  {
  public:
    char const* get_name() const noexcept { return this->name; }
    char const* get_unit() const noexcept { return this->unit; }

  protected:
    consteval column_description_t(char const* name, char const* unit)
        : name{ name }
        , unit{ unit }
    {
    }

    char const* name;
    char const* unit;
  };

  class column_description_int final: public column_description_t
  {
  public:
    using Format = logginator::formator::IntergerFormat;

    consteval column_description_int(char const* name, char const* unit, Format fmt)
        : column_description_t(name, unit)
        , fmt{ fmt }
    {
    }

    constexpr Format get_format() const noexcept { return this->fmt; }

  private:
    Format fmt;
  };

  class column_description_float final: public column_description_t
  {
  public:
    using Format = logginator::formator::FloatFormat;

    consteval column_description_float(char const* name, char const* unit, Format fmt)
        : column_description_t(name, unit)
        , fmt{ fmt }
    {
    }

    constexpr Format get_format() const noexcept { return this->fmt; }

  private:
    Format fmt;
  };
  struct column_description_binary final: public column_description_t
  {
  public:
    enum class Format
    {
      b64,
      default_fmt = b64,
    };
    consteval column_description_binary(char const* name, char const* unit, Format fmt)
        : column_description_t(name, unit)
        , fmt{ fmt }
    {
    }

    constexpr Format get_format() const noexcept { return this->fmt; }

  private:
    Format fmt;
  };

  struct column_description_string final: public column_description_t
  {
  public:
    enum class Format
    {
      ascii,
      default_fmt = ascii,
    };
    consteval column_description_string(char const* name, char const* unit, Format fmt)
        : column_description_t(name, unit)
        , fmt{ fmt }
    {
    }

    constexpr Format get_format() const noexcept { return this->fmt; }

  private:
    Format fmt;
  };

  class Manager_Interface
  {
  public:
    virtual ~Manager_Interface() = default;

    virtual void print_channels()                                           = 0;
    virtual void publish(std::string_view msg)                              = 0;
    virtual void subscribe(Printer_Interface& printer)                      = 0;
    virtual void unsubscribe(Printer_Interface& printer)                    = 0;
    virtual void setup_channel(uint8_t channel, uint32_t downsample_factor) = 0;
  };

  class Printer_Interface
  {
  public:
    virtual ~Printer_Interface() = default;

    virtual channel_description_t const& get_cfg() const                               = 0;
    virtual void                         print_header()                                = 0;
    virtual void                         setup(uint32_t downsample_factor)             = 0;
    virtual void                         publish(bool is_header, std::string_view msg) = 0;
  };

  class line_t
  {
  public:
    line_t(Printer_Interface& printer, char* begin, char* pos, char* end, bool print_header);
    ~line_t();

    template <typename T>
      requires(std::integral<T> && !std::same_as<T, bool>)
    bool add(column_description_int description, T const& value)
    {
      if (this->m_header)
      {
        return this->add(description);
      }
      auto ret = formator::append_int(this->m_pos, this->m_end, value, description.get_format());
      if (ret.ec != std::errc())
      {
        return false;
      }
      ret = formator::append_string(ret.ptr, this->m_end, ";");
      if (ret.ec != std::errc())
      {
        return false;
      }
      this->m_pos = ret.ptr;
      return true;
    }

    bool add(column_description_int description, bool const& value) { return this->add<char>(description, static_cast<char>(value)); }

    template <typename T>
      requires(std::floating_point<T>)
    bool add(column_description_float description, T const& value)
    {
      if (this->m_header)
      {
        return this->add(description);
      }
      auto ret = formator::append_float(this->m_pos, this->m_end, value, description.get_format());
      if (ret.ec != std::errc())
      {
        return false;
      }
      ret = formator::append_string(ret.ptr, this->m_end, ";");
      if (ret.ec != std::errc())
      {
        return false;
      }
      this->m_pos = ret.ptr;
      return true;
    }

    bool add(column_description_binary description, std::byte const& value) { return this->add(description, std::span<std::byte const>{ &value, 1 }); }
    bool add(column_description_binary description, std::span<std::byte const> value)
    {
      if (this->m_header)
      {
        return this->add(description);
      }
      auto ret = formator::append_base64(this->m_pos, this->m_end, value);
      if (ret.ec != std::errc())
      {
        return false;
      }
      ret = formator::append_string(ret.ptr, this->m_end, ";");
      if (ret.ec != std::errc())
      {
        return false;
      }
      this->m_pos = ret.ptr;
      return true;
    }

    bool add(column_description_string description, std::string_view value)
    {
      if (this->m_header)
      {
        return this->add(description);
      }
      auto ret = formator::append_string(this->m_pos, this->m_end, value);
      if (ret.ec != std::errc())
      {
        return false;
      }
      ret = formator::append_string(ret.ptr, this->m_end, ";");
      if (ret.ec != std::errc())
      {
        return false;
      }
      this->m_pos = ret.ptr;
      return true;
    }

  private:
    bool add(column_description_int description);
    bool add(column_description_float description);
    bool add(column_description_binary description);
    bool add(column_description_string description);

    bool add(std::string_view name, std::string_view unit, std::string_view format);

    Printer_Interface& m_printer;
    bool               m_header;
    char*              m_begin;
    char*              m_pos;
    char* const        m_end;
  };

  template <typename T> void print(T const& value, line_t& line) { return print(value, line); }

  template <typename T> line_t request_line(T const& value) { return request_line(value); }

  template <typename T>
    requires(std::destructible<T> && std::default_initializable<T> && std::is_copy_constructible_v<T>)
  void print(T const& value)
  {
    auto line = request_line(value);
    print(value, line);
  }
}    // namespace logginator

#endif    // !LOGGINATOR_UC_HPP_INCLUDED
