#pragma once
#ifndef LOGGINATOR_HPP_INCLUDED
#define LOGGINATOR_HPP_INCLUDED

#include <concepts>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

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
    enum class Format
    {
      ascii,
      hex,
      b64,
      default_fmt = ascii,
    };
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
    enum class Format
    {
      ascii,
      ascii_fixed,
      ascii_scientific,
      hex,
      b64,
      default_fmt = ascii,
    };
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

    virtual channel_description_t const& get_cfg() const                                                              = 0;
    virtual void                         print_header()                                                               = 0;
    virtual void                         setup(uint32_t downsample_factor)                                            = 0;
    virtual void                         publish(bool is_header)                                                      = 0;
    virtual void                         add(column_description_int const& description)                               = 0;
    virtual void                         add(column_description_float const& description)                             = 0;
    virtual void                         add(column_description_binary const& description)                            = 0;
    virtual void                         add(column_description_string const& description)                            = 0;
    virtual void                         add(bool const& value, column_description_int::Format fmt)                   = 0;
    virtual void                         add(char const& value, column_description_int::Format fmt)                   = 0;
    virtual void                         add(signed char const& value, column_description_int::Format fmt)            = 0;
    virtual void                         add(unsigned char const& value, column_description_int::Format fmt)          = 0;
    virtual void                         add(char8_t const& value, column_description_int::Format fmt)                = 0;
    virtual void                         add(char16_t const& value, column_description_int::Format fmt)               = 0;
    virtual void                         add(char32_t const& value, column_description_int::Format fmt)               = 0;
    virtual void                         add(signed short const& value, column_description_int::Format fmt)           = 0;
    virtual void                         add(unsigned short const& value, column_description_int::Format fmt)         = 0;
    virtual void                         add(signed int const& value, column_description_int::Format fmt)             = 0;
    virtual void                         add(unsigned int const& value, column_description_int::Format fmt)           = 0;
    virtual void                         add(signed long int const& value, column_description_int::Format fmt)        = 0;
    virtual void                         add(unsigned long int const& value, column_description_int::Format fmt)      = 0;
    virtual void                         add(signed long long int const& value, column_description_int::Format fmt)   = 0;
    virtual void                         add(unsigned long long int const& value, column_description_int::Format fmt) = 0;
    virtual void                         add(float const& value, column_description_float::Format fmt)                = 0;
    virtual void                         add(double const& value, column_description_float::Format fmt)               = 0;
    virtual void                         add(long double const& value, column_description_float::Format fmt)          = 0;
    virtual void                         add(std::byte const& value, column_description_binary::Format fmt)           = 0;
    virtual void                         add(std::span<std::byte const> value, column_description_binary::Format fmt) = 0;
    virtual void                         add(std::string_view value, column_description_string::Format fmt)           = 0;
  };

  template <typename T, typename D>
  concept is_supported_by_Printer_Interface = requires(T const& val, D const& des) {
    requires std::same_as<decltype(des.get_format()), typename D::Format>;
    std::declval<Printer_Interface&>().add(des);
    std::declval<Printer_Interface&>().add(val, des.get_format());
  };

  class line_t
  {
  public:
    line_t(Printer_Interface& printer, bool print_header);
    ~line_t();

    template <typename T, typename D>
      requires(is_supported_by_Printer_Interface<T, D>)
    void add(D const& description, T const& value)
    {
      if (this->m_header)
      {
        return this->m_printer.add(description);
      }
      return this->m_printer.add(value, description.get_format());
    }

  private:
    Printer_Interface& m_printer;
    bool               m_header;
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
