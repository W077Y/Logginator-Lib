#pragma once
#ifndef LOGGINATOR_HPP_INCLUDED
#define LOGGINATOR_HPP_INCLUDED

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <logginator-formator.hpp>
#include <span>
#include <string_view>
#include <type_traits>

namespace logginator
{
  class Manager_Interface;
  class Channel_Interface;
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

    virtual channel_description_t const& get_cfg() const                            = 0;
    virtual void                         publish(bool header, std::string_view msg) = 0;

    virtual void print_header()                    = 0;
    virtual void setup(uint32_t downsample_factor) = 0;
  };

  class line_t
  {
  public:
    line_t(Channel_Interface& channel, std::span<char> buffer, bool print_header);

    ~line_t();

    template <typename T>
      requires(std::integral<T> && !std::same_as<T, bool>)
    bool add(column_description_int description, T const& value)
    {
      if (this->m_header)
      {
        return this->add(description);
      }

      auto ret = formator::append_int(this->m_pos, this->m_end, value, description.get_format());
      if (ret.ec != std::errc())
      {
        return false;
      }
      ret = formator::append_string(ret.ptr, this->m_end, ";");
      if (ret.ec != std::errc())
      {
        return false;
      }
      this->m_pos = ret.ptr;
      return true;
    }

    bool add(column_description_int description, bool const& value) { return this->add<char>(description, static_cast<char>(value)); }

    template <typename T>
      requires(std::floating_point<T>)
    bool add(column_description_float description, T const& value)
    {
      if (this->m_header)
      {
        return this->add(description);
      }
      auto ret = formator::append_float(this->m_pos, this->m_end, value, description.get_format());
      if (ret.ec != std::errc())
      {
        return false;
      }
      ret = formator::append_string(ret.ptr, this->m_end, ";");
      if (ret.ec != std::errc())
      {
        return false;
      }
      this->m_pos = ret.ptr;
      return true;
    }

    bool add(column_description_binary description, std::byte const& value) { return this->add(description, std::span<std::byte const>{ &value, 1 }); }

    bool add(column_description_binary description, std::span<std::byte const> value)
    {
      if (this->m_header)
      {
        return this->add(description);
      }
      auto ret = formator::append_base64(this->m_pos, this->m_end, value);
      if (ret.ec != std::errc())
      {
        return false;
      }
      ret = formator::append_string(ret.ptr, this->m_end, ";");
      if (ret.ec != std::errc())
      {
        return false;
      }
      this->m_pos = ret.ptr;
      return true;
    }

    bool add(column_description_string description, std::string_view value)
    {
      if (this->m_header)
      {
        return this->add(description);
      }
      auto ret = formator::append_string(this->m_pos, this->m_end, value);
      if (ret.ec != std::errc())
      {
        return false;
      }
      ret = formator::append_string(ret.ptr, this->m_end, ";");
      if (ret.ec != std::errc())
      {
        return false;
      }
      this->m_pos = ret.ptr;
      return true;
    }

  private:
    bool add(column_description_int description);
    bool add(column_description_float description);
    bool add(column_description_binary description);
    bool add(column_description_string description);

    bool add(std::string_view name, std::string_view unit, std::string_view format);

    Channel_Interface& m_channel;
    bool               m_header;
    char*              m_begin;
    char*              m_pos;
    char* const        m_end;
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

      void publish(bool is_header, std::string_view msg) override
      {
        this->p_publish(is_header, msg);
        this->m_man.unlock_buffer();
      }

      line_t request_line(bool is_header)
      {
        this->m_man.lock_buffer();
        line_t line{ *this, this->m_buffer, is_header };
        return line;
      };

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
      static void print_column_description(line_t& line) { return logginator::print(T{}, line); };
    };

  public:
    class Output_Interface
    {
    public:
      virtual void operator()(std::string_view) = 0;
    };

    Manager_Interface(std::span<char> buffer)
        : m_buffer(buffer)
    {
    }

    virtual ~Manager_Interface() = default;

    virtual void print_channels()                                           = 0;
    virtual void setup_channel(uint8_t channel, uint32_t downsample_factor) = 0;

    template <typename T> Channel<T> request_channel(T const&, channel_description_t const& cfg) { return Channel<T>{ *this, cfg, this->m_buffer }; }

  private:
    virtual void publish(std::string_view msg)           = 0;
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

    Output_Interface&                                      m_out;
    std::array<Channel_Interface*, max_number_of_channels> m_list{};

    void publish(std::string_view msg) override { return this->m_out(msg); }

    void print_channels() override
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

    void subscribe(Channel_Interface& channel) override
    {
      auto& ent = this->m_list.at(channel.get_cfg().ID);
      if (ent != nullptr)
      {
        throw std::exception();
      }

      ent = &channel;
    }

    void unsubscribe(Channel_Interface& channel) override
    {
      auto& ent = this->m_list.at(channel.get_cfg().ID);
      if (ent != &channel)
      {
        throw std::exception();
      }

      ent = nullptr;
    }

    void setup_channel(uint8_t channel, uint32_t downsample_factor) override
    {
      auto& ent = this->m_list.at(channel);
      if (ent == nullptr)
      {
        throw std::exception();
      }

      ent->setup(downsample_factor);
    }
  };

  template <typename MutexType, std::size_t Buffer_Size> class Manager final: public Manager_Base
  {
  public:
    Manager(Output_Interface& output)
        : Manager_Base(output, this->m_buffer)
    {
    }

    void lock_buffer() override { this->m_mutex.lock(); }
    void unlock_buffer() override { this->m_mutex.unlock(); }

  private:
    MutexType                     m_mutex;
    std::array<char, Buffer_Size> m_buffer{};
  };
}    // namespace logginator

#endif    // !LOGGINATOR_UC_HPP_INCLUDED
