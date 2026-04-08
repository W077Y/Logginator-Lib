#include <exception>
#include <logginator-manager.hpp>

namespace logginator
{
  Manager::Manager(Output_Interface& output)
      : m_out(output)
  {
  }

  void Manager::publish(std::string_view msg) { return this->m_out(msg); }

  void Manager::print_channels()
  {
    for (auto& ent : this->m_list)
    {
      if (ent == nullptr)
      {
        continue;
      }

      ent->print_header();
    }
  }

  void Manager::subscribe(Printer_Interface& printer)
  {
    auto& ent = this->m_list.at(printer.get_cfg().ID);
    if (ent != nullptr)
    {
      throw std::exception();
    }

    ent = &printer;
  }

  void Manager::unsubscribe(Printer_Interface& printer)
  {
    auto& ent = this->m_list.at(printer.get_cfg().ID);
    if (ent != &printer)
    {
      throw std::exception();
    }

    ent = nullptr;
  }

  void Manager::setup_channel(uint8_t channel, uint32_t downsample_factor)
  {
    auto& ent = this->m_list.at(channel);
    if (ent == nullptr)
    {
      throw std::exception();
    }

    ent->setup(downsample_factor);
  }

}    // namespace logginator
