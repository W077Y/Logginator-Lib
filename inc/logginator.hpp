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
  concept ChannelID = std::is_enum_v<T> && std::is_same_v<std::underlying_type_t<T>, uint8_t>;

  struct ChannelDescription
  {
    template <ChannelID T>
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

    class downsampler_t
    {
    public:
      using counting_type = uint8_t;

      downsampler_t() = default;
      downsampler_t(counting_type target)
          : m_trg(target)
      {
      }

      bool is_ready() const
      {
        if (this->m_trg == 0)
          return false;

        if (this->m_cnt != 0)
          return false;

        return true;
      }

      void tick()
      {
        ++this->m_cnt;
        if (this->m_cnt >= this->m_trg)
        {
          this->m_cnt = 0;
        }
      }

      bool poll()
      {
        auto ret = this->is_ready();
        this->tick();
        return ret;
      }

      void set_trg(counting_type target)
      {
        this->m_trg = target;
        this->m_cnt = 0;
      }

    private:
      counting_type m_trg = 0;
      counting_type m_cnt = 0;
    };
  }    // namespace detail

  using ColumnDescriptionInt    = detail::column_description_format<format::IntegerFormat>;
  using ColumnDescriptionFloat  = detail::column_description_format<format::FloatFormat>;
  using ColumnDescriptionBinary = detail::column_description_format<format::BinaryFormat>;
  using ColumnDescriptionString = detail::column_description_format<format::StringFormat>;

  class Lockable_Publisher_Interface
  {
  public:
    virtual ~Lockable_Publisher_Interface() = default;

    virtual void lock()   = 0;
    virtual void unlock() = 0;

    virtual void publish(std::string_view msg) = 0;

    class PublisherLock
    {
    public:
      PublisherLock(Lockable_Publisher_Interface& pub)
          : m_pub(pub)
      {
        this->m_pub.lock();
      }

      PublisherLock(PublisherLock const&)            = delete;
      PublisherLock(PublisherLock&&)                 = delete;
      PublisherLock& operator=(PublisherLock const&) = delete;
      PublisherLock& operator=(PublisherLock&&)      = delete;

      ~PublisherLock() { this->m_pub.unlock(); }

      auto publish(std::string_view msg) { return this->m_pub.publish(msg); }

    private:
      Lockable_Publisher_Interface& m_pub;
    };
  };

  class line_t
  {
  public:
    line_t(line_t const&)            = delete;
    line_t(line_t&&)                 = delete;
    line_t& operator=(line_t const&) = delete;
    line_t& operator=(line_t&&)      = delete;

    line_t(Lockable_Publisher_Interface& publisher,       //
           uint8_t                       id,              //
           std::string_view              name,            //
           std::span<char>               buffer,          //
           bool                          print_header,    //
           detail::downsampler_t&        downsampler);

    ~line_t();

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

    Lockable_Publisher_Interface::PublisherLock m_pub;
    bool                                        m_header;
    bool                                        m_publish;
    char*                                       m_begin;
    char*                                       m_pos;
    char* const                                 m_end;
  };

  class Channel_Interface: protected Lockable_Publisher_Interface
  {
  public:
    using print_header_fnc = void (&)(line_t&);
    using counting_type    = detail::downsampler_t::counting_type;
    Channel_Interface(ChannelDescription cfg, print_header_fnc header_fnc, counting_type downsample_factor)
        : m_cfg(cfg)
        , m_header_fnc(header_fnc)
        , m_downsampler(downsample_factor)
    {
    }

    Channel_Interface(Channel_Interface const&)            = delete;
    Channel_Interface(Channel_Interface&&)                 = delete;
    Channel_Interface& operator=(Channel_Interface const&) = delete;
    Channel_Interface& operator=(Channel_Interface&&)      = delete;

    virtual ~Channel_Interface() = default;

    line_t request_line() & { return this->request_line(false); }
    void   print_header() &
    {
      auto line = this->request_line(true);
      this->m_header_fnc(line);
    }
    void                      setup(counting_type downsample_factor) & { return this->m_downsampler.set_trg(downsample_factor); }
    ChannelDescription const& get_cfg() const& { return this->m_cfg; }

  protected:
    template <typename T> static void print_column_description(line_t& line) { return print(T{}, line); };

    virtual line_t request_line(bool header) & = 0;

    ChannelDescription const m_cfg;
    print_header_fnc const   m_header_fnc;
    detail::downsampler_t    m_downsampler = {};
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
    class Channel final: public Channel_Interface
    {
    public:
      using Channel_Interface::request_line;

      template <typename T>
      Channel(Manager_Interface& man, std::span<char> buffer, ChannelDescription cfg, T const&, counting_type downsample_factor)
          : Channel_Interface(cfg, Channel_Interface::print_column_description<T>, downsample_factor)
          , m_man(man)
          , m_buffer(buffer)
      {
        this->m_man.subscribe(*this);
      }
      ~Channel() { this->m_man.unsubscribe(*this); }

    private:
      void   publish(std::string_view msg) override { this->m_man.publish(msg); }
      line_t request_line(bool is_header) & override
      {
        return line_t{ *this, this->m_cfg.ID, this->m_cfg.name, this->m_buffer, is_header, this->m_downsampler };
      };
      void lock() override { this->m_man.lock_buffer(); };
      void unlock() override { this->m_man.unlock_buffer(); };

      Manager_Interface& m_man;
      std::span<char>    m_buffer = {};
    };

  public:
    using counting_type = detail::downsampler_t::counting_type;

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

    virtual void print_channels()                                                = 0;
    virtual void setup_channel(uint8_t channel, counting_type downsample_factor) = 0;

    template <ChannelID T> void setup_channel(T channel, counting_type downsample_factor) { setup_channel(std::to_underlying(channel), downsample_factor); }

    template <typename T> Channel request_channel(T const& val, ChannelDescription const& cfg, counting_type downsample_factor = 0)
    {
      return Channel{ *this, this->m_buffer, cfg, val, downsample_factor };
    }

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

    void setup_channel(uint8_t channel, counting_type downsample_factor) override
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
