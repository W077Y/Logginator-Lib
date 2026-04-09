#pragma once
#ifndef LOGGINATOR_ERROR_HPP_INCLUDED
#define LOGGINATOR_ERROR_HPP_INCLUDED

#include <exception>

namespace logginator::errors
{

  struct logginator_error: std::exception
  {
    const char* what() const noexcept override { return "logginator error"; }
  };

  struct channel_subscribtion_error: logginator_error
  {
    const char* what() const noexcept override { return "channel already taken or invalid"; }
  };

  struct channel_setup_error: logginator_error
  {
    const char* what() const noexcept override { return "channel setup error"; }
  };

  struct line_serialization_error: logginator_error
  {
    const char* what() const noexcept override { return "line serialization error"; }
  };
}    // namespace logginator::errors

#endif    // !LOGGINATOR_ERROR_HPP_INCLUDED