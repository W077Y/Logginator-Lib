#pragma once
#ifndef LOGGINATOR_HPP_INCLUDED
#define LOGGINATOR_HPP_INCLUDED

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <logginator-error.hpp>
#include <logginator-formator.hpp>
#include <span>
#include <string_view>

namespace logginator
{
  class Manager_Interface;
  class Manager_Base;

  class Channel_Interface;
  class line_t;

  struct channel_description_t
  {
    consteval channel_description_t(uint8_t id, char const* name)
        : ID{ id }
        , name{ name }
    {
    }

    uint8_t                ID;
    std::string_view const name;
  };

  class column_description_t
  {
  public:
    std::string_view get_name() const noexcept { return this->name; }
    std::string_view get_unit() const noexcept { return this->unit; }

  protected:
    consteval column_description_t(char const* name, char const* unit)
        : name{ name }
        , unit{ unit }
    {
    }

    std::string_view const name;
    std::string_view const unit;
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

  class Channel_Interface
  {
  public:
    virtual ~Channel_Interface() = default;

  protected:
    friend line_t;
    friend Manager_Base;

    virtual channel_description_t const& get_cfg() const                            = 0;
    virtual void                         publish(bool header, std::string_view msg) = 0;

    virtual void print_header()                    = 0;
    virtual void setup(uint32_t downsample_factor) = 0;

    virtual void lock_buffer()   = 0;
    virtual void unlock_buffer() = 0;

    struct ChannelLock
    {
      ChannelLock(Channel_Interface& ch)
          : channel(ch)
      {
        this->channel.lock_buffer();
      }

      ChannelLock(ChannelLock const&)            = delete;
      ChannelLock(ChannelLock&&)                 = delete;
      ChannelLock& operator=(ChannelLock const&) = delete;
      ChannelLock& operator=(ChannelLock&&)      = delete;

      ~ChannelLock() { this->channel.unlock_buffer(); }

      Channel_Interface& channel;
    };
  };

  class line_t
  {
  public:
    line_t(Channel_Interface& channel, std::span<char> buffer, bool print_header);

    line_t(line_t const&)            = delete;
    line_t(line_t&&)                 = delete;
    line_t& operator=(line_t const&) = delete;
    line_t& operator=(line_t&&)      = delete;

    ~line_t();

    channel_description_t const& get_cfg() const { return this->m_channel.channel.get_cfg(); }

    template <typename T>
      requires(std::integral<T> && !std::same_as<T, bool>)
    void add(column_description_int description, T const& value)
    {
      if (this->m_header)
      {
        return this->add(description);
      }

      auto ret = formator::append_int(this->m_pos, this->m_end, value, description.get_format());
      if (ret.ec != std::errc())
      {
        throw logginator::errors::line_serialization_error();
      }
      ret = formator::append_string(ret.ptr, this->m_end, ";");
      if (ret.ec != std::errc())
      {
        throw logginator::errors::line_serialization_error();
      }
      this->m_pos = ret.ptr;
    }

    void add(column_description_int description, bool const& value) { return this->add<char>(description, static_cast<char>(value)); }

    template <typename T>
      requires(std::floating_point<T>)
    void add(column_description_float description, T const& value)
    {
      if (this->m_header)
      {
        return this->add(description);
      }
      auto ret = formator::append_float(this->m_pos, this->m_end, value, description.get_format());
      if (ret.ec != std::errc())
      {
        throw logginator::errors::line_serialization_error();
      }
      ret = formator::append_string(ret.ptr, this->m_end, ";");
      if (ret.ec != std::errc())
      {
        throw logginator::errors::line_serialization_error();
      }
      this->m_pos = ret.ptr;
    }

    void add(column_description_binary description, std::byte const& value) { return this->add(description, std::span<std::byte const>{ &value, 1 }); }

    void add(column_description_binary description, std::span<std::byte const> value)
    {
      if (this->m_header)
      {
        return this->add(description);
      }
      auto ret = formator::append_base64(this->m_pos, this->m_end, value);
      if (ret.ec != std::errc())
      {
        throw logginator::errors::line_serialization_error();
      }
      ret = formator::append_string(ret.ptr, this->m_end, ";");
      if (ret.ec != std::errc())
      {
        throw logginator::errors::line_serialization_error();
      }
      this->m_pos = ret.ptr;
    }

    void add(column_description_string description, std::string_view value)
    {
      if (this->m_header)
      {
        return this->add(description);
      }
      auto ret = formator::append_string(this->m_pos, this->m_end, value);
      if (ret.ec != std::errc())
      {
        throw logginator::errors::line_serialization_error();
      }
      ret = formator::append_string(ret.ptr, this->m_end, ";");
      if (ret.ec != std::errc())
      {
        throw logginator::errors::line_serialization_error();
      }
      this->m_pos = ret.ptr;
    }

  private:
    void add(column_description_int description);
    void add(column_description_float description);
    void add(column_description_binary description);
    void add(column_description_string description);

    void add(std::string_view name, std::string_view unit, std::string_view format);

    Channel_Interface::ChannelLock m_channel;
    bool                           m_header;
    char*                          m_begin;
    char*                          m_pos;
    char* const                    m_end;
  };

  namespace detail
  {
    // ADL Forwarding: These calls will find user-provided implementations
    // via Argument-Dependent Lookup based on the template parameter T
    template <typename T> void adl_print(T const& value, line_t& line) { return print(value, line); }    // Looks for: my_app::print via ADL

    template <typename T> line_t adl_request_line(T const& value) { return request_line(value); }
  }    // namespace detail

  template <typename T> void   print(T const& value, line_t& line);    // Declaration only!
  template <typename T> line_t request_line(T const& value);
  // NO bodies here - users provide these in their namespace and they will be found via ADL

  template <typename T>
    requires std::destructible<T> && std::default_initializable<T> && requires(const T& value, line_t& line) {
      { detail::adl_print(value, line) } -> std::same_as<void>;
      { detail::adl_request_line(value) } -> std::same_as<line_t>;
    }
  void print(T const& value)
  {
    auto line = detail::adl_request_line(value);
    detail::adl_print(value, line);
  }

  class Manager_Interface
  {
    class Channel_Base: public Channel_Interface
    {
    public:
      line_t request_line() { return request_line(false); }

    protected:
      using print_header_fnc = void (&)(line_t&);
      Channel_Base(Manager_Interface& man, std::span<char> buffer, channel_description_t cfg, print_header_fnc fnc)
          : m_man(man)
          , m_default_msg(fnc)
          , m_cfg(cfg)
          , m_buffer(buffer)
      {
      }

      channel_description_t const& get_cfg() const override { return this->m_cfg; }

      void print_header() override
      {
        auto line = this->request_line(true);
        this->m_default_msg(line);
      }

      void setup(uint32_t downsample_factor) override
      {
        this->m_downsample_trg = downsample_factor;
        this->m_downsample_cnt = downsample_factor;
      }

      void publish(bool is_header, std::string_view msg) override { this->p_publish(is_header, msg); }

      line_t request_line(bool is_header) { return line_t{ *this, this->m_buffer, is_header }; };

    private:
      void p_publish(bool is_header, std::string_view msg)
      {
        if (is_header)
        {
          return this->m_man.publish(msg);
        }

        if (this->m_downsample_trg == 0)
        {
          return;
        }

        if (++this->m_downsample_cnt >= this->m_downsample_trg)
        {
          this->m_downsample_cnt = 0;
        }

        if (this->m_downsample_cnt != 0)
        {
          return;
        }

        this->m_man.publish(msg);
      }

      void lock_buffer() override { this->m_man.lock_buffer(); };
      void unlock_buffer() override { this->m_man.unlock_buffer(); };

    protected:
      Manager_Interface&          m_man;
      print_header_fnc            m_default_msg;
      channel_description_t const m_cfg;
      std::span<char>             m_buffer         = {};
      uint32_t                    m_downsample_trg = 0;
      uint32_t                    m_downsample_cnt = 0;
    };

    template <typename T> class Channel final: public Channel_Base
    {
    public:
      Channel(Manager_Interface& man, channel_description_t const& cfg, std::span<char> buffer)
          : Channel_Base(man, buffer, cfg, print_column_description)
      {
        man.subscribe(*this);
      }

      ~Channel() { this->m_man.unsubscribe(*this); }

    private:
      static void print_column_description(line_t& line) { return print(T{}, line); };
    };

  public:
    class Output_Interface
    {
    public:
      virtual void operator()(std::string_view) noexcept = 0;
    };

    Manager_Interface(std::span<char> buffer)
        : m_buffer(buffer)
    {
    }

    virtual ~Manager_Interface() = default;

    virtual void print_channels()                                           = 0;
    virtual void setup_channel(uint8_t channel, uint32_t downsample_factor) = 0;

    template <typename T> Channel<T> request_channel(T const&, channel_description_t const& cfg) { return Channel<T>{ *this, cfg, this->m_buffer }; }

  protected:
    virtual void publish(std::string_view msg) noexcept  = 0;
    virtual void subscribe(Channel_Interface& channel)   = 0;
    virtual void unsubscribe(Channel_Interface& channel) = 0;
    virtual void lock_buffer()                           = 0;
    virtual void unlock_buffer()                         = 0;

    std::span<char> m_buffer;
  };

  class Manager_Base: public Manager_Interface
  {
  protected:
    Manager_Base(Output_Interface& out, std::span<char> buffer)
        : Manager_Interface(buffer)
        , m_out(out)
    {
    }

  private:
    static constexpr std::size_t max_number_of_channels = std::numeric_limits<uint8_t>::max() + 1;

    struct list_lock_guard
    {
      list_lock_guard(Manager_Base& man)
          : m_man(man)
      {
        this->m_man.lock_list();
      }

      list_lock_guard(list_lock_guard const&)            = delete;
      list_lock_guard(list_lock_guard&&)                 = delete;
      list_lock_guard& operator=(list_lock_guard const&) = delete;
      list_lock_guard& operator=(list_lock_guard&&)      = delete;

      ~list_lock_guard() { this->m_man.unlock_list(); }

      Manager_Base& m_man;
    };

    void publish(std::string_view msg) noexcept override { return this->m_out(msg); }

    void print_channels() override
    {
      list_lock_guard lock(*this);
      for (auto& ent : this->m_list)
      {
        if (ent == nullptr)
        {
          continue;
        }

        ent->print_header();
      }
    }

    void subscribe(Channel_Interface& channel) override
    {
      list_lock_guard lock(*this);
      auto&           ent = this->m_list.at(channel.get_cfg().ID);
      if (ent != nullptr)
      {
        this->unlock_list();
        throw logginator::errors::channel_subscribtion_error();
      }

      ent = &channel;
      this->unlock_list();
    }

    void unsubscribe(Channel_Interface& channel) override
    {
      list_lock_guard lock(*this);
      auto&           ent = this->m_list.at(channel.get_cfg().ID);
      if (ent != &channel)
      {
        throw logginator::errors::channel_subscribtion_error();
      }

      ent = nullptr;
    }

    void setup_channel(uint8_t channel, uint32_t downsample_factor) override
    {
      list_lock_guard lock(*this);
      auto&           ent = this->m_list.at(channel);
      if (ent == nullptr)
      {
        throw logginator::errors::channel_setup_error();
      }

      ent->setup(downsample_factor);
    }

    virtual void lock_list()   = 0;
    virtual void unlock_list() = 0;

    Output_Interface&                                      m_out;
    std::array<Channel_Interface*, max_number_of_channels> m_list{};
  };

  template <typename MutexType, std::size_t Buffer_Size> class Manager final: public Manager_Base
  {
  public:
    Manager(Output_Interface& output)
        : Manager_Base(output, this->m_buffer)
    {
    }

    void lock_buffer() override { this->m_mutex_buffer.lock(); }
    void unlock_buffer() override { this->m_mutex_buffer.unlock(); }

    void lock_list() override { this->m_mutex_list.lock(); }
    void unlock_list() override { this->m_mutex_list.unlock(); }

  private:
    MutexType                     m_mutex_buffer;
    MutexType                     m_mutex_list;
    std::array<char, Buffer_Size> m_buffer{};
  };
}    // namespace logginator

#endif    // !LOGGINATOR_HPP_INCLUDED
