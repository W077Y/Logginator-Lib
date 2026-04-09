#pragma once
#ifndef LOGGINATOR_PRINTER_HPP_INCLUDED
#define LOGGINATOR_PRINTER_HPP_INCLUDED

#include <cstdint>
#include <logginator.hpp>

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
    void                         publish(bool is_header, std::string_view msg) override;

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
