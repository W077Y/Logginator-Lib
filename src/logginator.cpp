#include <logginator.hpp>

namespace logginator
{
  line_t::line_t(Printer_Interface& printer, bool header)
      : m_printer(printer)
      , m_header(header)
  {
  }
  line_t::~line_t() { this->m_printer.publish(this->m_header); }
}    // namespace logginator
