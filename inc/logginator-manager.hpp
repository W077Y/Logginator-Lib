#pragma once
#include <cstdint>
#include <limits>
#ifndef LOGGINATOR_MANAGER_HPP_INCLUDED
#define LOGGINATOR_MANAGER_HPP_INCLUDED

#include <array>
#include <logginator.hpp>
#include <string_view>

namespace logginator
{
  class Manager final: public Manager_Interface
  {
  public:
    class Output_Interface
    {
    public:
      virtual void operator()(std::string_view) = 0;
    };

    Manager(Output_Interface& output);

    void print_channels() override;

    void publish(std::string_view msg) override;
    void subscribe(Printer_Interface&) override;
    void unsubscribe(Printer_Interface&) override;
    void setup_channel(uint8_t channel, uint32_t downsample_factor) override;

  private:
    static constexpr std::size_t max_number_of_channels = std::numeric_limits<uint8_t>::max() + 1;

    Output_Interface&                                      m_out;
    std::array<Printer_Interface*, max_number_of_channels> m_list{};
  };
}    // namespace logginator

#endif
