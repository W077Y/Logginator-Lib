#pragma once
#ifndef LOGGINATOR_HPP_INCLUDED
#define LOGGINATOR_HPP_INCLUDED

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <logginator-error.hpp>
#include <logginator-format.hpp>
#include <span>
#include <string_view>
#include <utility>

namespace logginator
{
  class Manager_Interface;
  class Manager_Base;

  class Channel_Interface;
  class line_t;

  template <typename T>
  concept channel_id_t = std::is_enum_v<T> && std::is_same_v<std::underlying_type_t<T>, uint8_t>;

  struct ChannelDescription
  {
    template <typename T> requires channel_id_t<T>
    consteval ChannelDescription(T id, std::string_view name)
        : ChannelDescription(std::to_underlying(id), name)
    {
    }

    consteval ChannelDescription(uint8_t id, std::string_view name)
        : ID{ id }
        , name{ name }
    {
    }

    uint8_t                ID;
    std::string_view const name;
  };

  namespace detail
  {
    class column_description_t
    {
    public:
      constexpr std::string_view get_name() const noexcept { return this->name; }
      constexpr std::string_view get_unit() const noexcept { return this->unit; }

    protected:
      consteval column_description_t(std::string_view name, std::string_view unit)
          : name{ name }
          , unit{ unit }
      {
      }

      std::string_view const name;
      std::string_view const unit;
    };

    template <typename T>
      requires(std::is_enum_v<T>)
    class column_description_format final: public column_description_t
    {
    public:
      using Format = T;

      consteval column_description_format(std::string_view name, std::string_view unit, Format fmt)
          : column_description_t(name, unit)
          , fmt{ fmt }
      {
      }

      constexpr Format get_format() const noexcept { return this->fmt; }

    private:
      Format const fmt;
    };
  }    // namespace detail

  using ColumnDescriptionInt    = detail::column_description_format<format::IntegerFormat>;
  using ColumnDescriptionFloat  = detail::column_description_format<format::FloatFormat>;
  using ColumnDescriptionBinary = detail::column_description_format<format::BinaryFormat>;
  using ColumnDescriptionString = detail::column_description_format<format::StringFormat>;

  class Channel_Interface
  {
  public:
    virtual ~Channel_Interface() = default;

  protected:
    friend line_t;
    friend Manager_Base;

    virtual ChannelDescription const& get_cfg() const&                             = 0;
    virtual void                      publish(bool header, std::string_view msg) & = 0;
    virtual void                      print_header() &                             = 0;
    virtual void                      setup(uint32_t downsample_factor) &          = 0;
    virtual void                      lock_buffer() &                              = 0;
    virtual void                      unlock_buffer() &                            = 0;

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
    line_t(line_t const&)            = delete;
    line_t(line_t&&)                 = delete;
    line_t& operator=(line_t const&) = delete;
    line_t& operator=(line_t&&)      = delete;

    line_t(Channel_Interface& channel, std::span<char> buffer, bool print_header);
    ~line_t();

    ChannelDescription const& get_cfg() const& { return this->m_channel.channel.get_cfg(); }

    template <typename D, typename T>
      requires((std::same_as<D, ColumnDescriptionInt> && std::integral<T> && !std::same_as<T, bool>) ||
               (std::same_as<D, ColumnDescriptionFloat> && std::floating_point<T>))
    void add(D const& description, T const& value) &
    {
      if (this->m_header)
      {
        return this->add(description);
      }

      auto ret = format::append(this->m_pos, this->m_end, value, description.get_format());
      if (ret.ec != std::errc())
      {
        throw logginator::errors::line_serialization_error();
      }
      ret = format::append(ret.ptr, this->m_end, ";", format::StringFormat::ascii);
      if (ret.ec != std::errc())
      {
        throw logginator::errors::line_serialization_error();
      }
      this->m_pos = ret.ptr;
    }

    void add(ColumnDescriptionInt const& description, bool const& value) & { return this->add(description, static_cast<char>(value)); }

    void add(ColumnDescriptionBinary const& description, std::byte const& value) & { return this->add(description, std::span<std::byte const>{ &value, 1 }); }

    template <typename D, typename T>
      requires(std::same_as<D, ColumnDescriptionBinary> && std::convertible_to<T, std::span<std::byte const>>) ||
              (std::same_as<D, ColumnDescriptionString> && std::convertible_to<T, std::string_view>)
    void add(D const& description, T value) &
    {
      if (this->m_header)
      {
        return this->add(description);
      }

      auto ret = format::append(this->m_pos, this->m_end, value, description.get_format());
      if (ret.ec != std::errc())
      {
        throw logginator::errors::line_serialization_error();
      }

      ret = format::append(ret.ptr, this->m_end, ";", format::StringFormat::ascii);
      if (ret.ec != std::errc())
      {
        throw logginator::errors::line_serialization_error();
      }
      this->m_pos = ret.ptr;
    }

  private:
    void add(ColumnDescriptionInt const& description) &;
    void add(ColumnDescriptionFloat const& description) &;
    void add(ColumnDescriptionBinary const& description) &;
    void add(ColumnDescriptionString const& description) &;

    void add(std::string_view name, std::string_view unit, std::string_view format) &;

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
      Channel_Base(Manager_Interface& man, std::span<char> buffer, ChannelDescription cfg, print_header_fnc fnc)
          : m_man(man)
          , m_default_msg(fnc)
          , m_cfg(cfg)
          , m_buffer(buffer)
      {
      }

      ChannelDescription const& get_cfg() const& override { return this->m_cfg; }

      void print_header() & override
      {
        auto line = this->request_line(true);
        this->m_default_msg(line);
      }

      void setup(uint32_t downsample_factor) & override
      {
        this->m_downsample_trg = downsample_factor;
        this->m_downsample_cnt = downsample_factor;
      }

      void publish(bool is_header, std::string_view msg) & override { this->p_publish(is_header, msg); }

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

      void lock_buffer() & override { this->m_man.lock_buffer(); };
      void unlock_buffer() & override { this->m_man.unlock_buffer(); };

    protected:
      Manager_Interface&       m_man;
      print_header_fnc         m_default_msg;
      ChannelDescription const m_cfg;
      std::span<char>          m_buffer         = {};
      uint32_t                 m_downsample_trg = 0;
      uint32_t                 m_downsample_cnt = 0;
    };

    template <typename T> class Channel final: public Channel_Base
    {
    public:
      Channel(Manager_Interface& man, ChannelDescription const& cfg, std::span<char> buffer)
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
    
    template <typename T> requires channel_id_t<T>
    void setup_channel(T channel, uint32_t downsample_factor)
    {
      setup_channel(std::to_underlying(channel), downsample_factor);
    }

    template <typename T> Channel<T> request_channel(T const&, ChannelDescription const& cfg) { return Channel<T>{ *this, cfg, this->m_buffer }; }

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
        throw logginator::errors::channel_subscribtion_error();
      }

      ent = &channel;
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
