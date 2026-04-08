#pragma once
#include <cstdint>
#ifndef LOGGINATOR_PRINTER_HPP_INCLUDED
#define LOGGINATOR_PRINTER_HPP_INCLUDED

#include <logginator.hpp>
#include <string_view>

namespace logginator
{
  class Printer_Base: public Printer_Interface
  {
  public:
    using print_header_fnc = void (&)(line_t&);

    line_t request_line();

  protected:
    Printer_Base(Manager_Interface& man, std::span<char> buffer, channel_description_t cfg, print_header_fnc fnc);

    channel_description_t const& get_cfg() const override;
    line_t                       request_line(bool is_header);
    void                         print_header() override;
    void                         setup(uint32_t downsample_factor) override;
    void                         publish(bool is_header) override;
    virtual void                 add(column_description_int const& description) override;
    virtual void                 add(column_description_float const& description) override;
    virtual void                 add(column_description_binary const& description) override;
    virtual void                 add(column_description_string const& description) override;
    virtual void                 add(bool const& value, column_description_int::Format fmt) override;
    virtual void                 add(char const& value, column_description_int::Format fmt) override;
    virtual void                 add(signed char const& value, column_description_int::Format fmt) override;
    virtual void                 add(unsigned char const& value, column_description_int::Format fmt) override;
    virtual void                 add(char8_t const& value, column_description_int::Format fmt) override;
    virtual void                 add(char16_t const& value, column_description_int::Format fmt) override;
    virtual void                 add(char32_t const& value, column_description_int::Format fmt) override;
    virtual void                 add(signed short const& value, column_description_int::Format fmt) override;
    virtual void                 add(unsigned short const& value, column_description_int::Format fmt) override;
    virtual void                 add(signed int const& value, column_description_int::Format fmt) override;
    virtual void                 add(unsigned int const& value, column_description_int::Format fmt) override;
    virtual void                 add(signed long int const& value, column_description_int::Format fmt) override;
    virtual void                 add(unsigned long int const& value, column_description_int::Format fmt) override;
    virtual void                 add(signed long long int const& value, column_description_int::Format fmt) override;
    virtual void                 add(unsigned long long int const& value, column_description_int::Format fmt) override;
    virtual void                 add(float const& value, column_description_float::Format fmt) override;
    virtual void                 add(double const& value, column_description_float::Format fmt) override;
    virtual void                 add(long double const& value, column_description_float::Format fmt) override;
    virtual void                 add(std::byte const& value, column_description_binary::Format fmt) override;
    virtual void                 add(std::span<std::byte const> value, column_description_binary::Format fmt) override;
    virtual void                 add(std::string_view value, column_description_string::Format fmt) override;

    virtual void lock_line()   = 0;
    virtual void unlock_line() = 0;

    Manager_Interface&          m_man;
    print_header_fnc            m_default_msg;
    channel_description_t const m_cfg;
    char* const                 m_buffer_begin;
    char* const                 m_buffer_end;
    char*                       m_pos            = nullptr;
    uint32_t                    m_downsample_trg = 0;
    uint32_t                    m_downsample_cnt = 0;
  };

  template <typename MutexType, typename T, std::size_t N> class Printer final: public Printer_Base
  {
  public:
    Printer(Manager_Interface& man, channel_description_t const& cfg)
        : Printer_Base(man, this->m_buffer, cfg, print_column_description)
    {
      man.subscribe(*this);
    }

    ~Printer() { this->m_man.unsubscribe(*this); }

  private:
    static void print_column_description(line_t& line) { return logginator::print(T{}, line); };

    void lock_line() override { this->m_mtex.lock(); }
    void unlock_line() override { this->m_mtex.unlock(); }

    std::array<char, N> m_buffer = {};
    MutexType           m_mtex;
  };

}    // namespace logginator

#endif
