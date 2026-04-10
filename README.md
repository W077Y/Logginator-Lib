# Logginator Library

A **type-safe, zero-allocation structured logging framework** for embedded systems and high-performance applications.

Logginator provides deterministic, thread-safe telemetry output with compile-time safety guarantees. All formatting happens with fixed-size buffers—no dynamic allocation, no surprises.

## Key Features

✅ **Zero Dynamic Allocation** — Fixed buffers via `std::span<char>`  
✅ **Thread-Safe** — Separate mutexes for buffer and channel list  
✅ **Compile-Time Type Safety** — Concepts & consteval constraints  
✅ **RAII Publishing** — Automatic flush on scope exit  
✅ **Channel-Based Routing** — Multiple independent channels (0-255)  
✅ **Per-Channel Downsampling** — Reduce output volume intelligently  
✅ **Custom Formats** — ASCII, Hex, Base64, Scientific notation  
✅ **ADL-Based Extensibility** — Add custom types without modifying library  
✅ **Minimal Overhead** — Suitable for real-time / embedded systems  

## Architecture

### Core Components

| Component | Purpose |
|-----------|---------|
| **Manager** | Central router; manages channels and publishes to output backend |
| **Channel** | Identified message stream with configurable downsampling |
| **line_t** | RAII wrapper for a single log line; ensures complete publishing |
| **Printer<T>** | Type-specific formatter registered on a channel |

### Format Specification

```
#<channel_id>;<name>[<unit>]{<format>};<value>;<value>;...
```

**Example:**
```
#42;temperature[°C]{ascii_fixed};pressure[Pa]{hex}
#42;23.50;0x1a2b3c4d
```

## Getting Started

### 1. Define Your Data Type

```cpp
namespace my_app {
  struct SensorData {
    std::size_t timestamp;      // milliseconds
    double temperature;         // Celsius
    double humidity;            // %
  };
}
```

### 2. Implement Formatting (ADL Hook)

Provide `print()` and `request_line()` in your namespace:

```cpp
namespace my_app {
  // Print a single measurement into the log line
  void print(SensorData const& data, logginator::line_t& line) {
    using namespace logginator;
    using FI = ColumnDescriptionInt::Format;
    using FF = ColumnDescriptionFloat::Format;
    
    line.add(ColumnDescriptionInt{"time", "ms", FI::ascii}, data.timestamp);
    line.add(ColumnDescriptionFloat{"temp", "°C", FF::ascii_fixed}, data.temperature);
    line.add(ColumnDescriptionFloat{"humidity", "%", FF::ascii_fixed}, data.humidity);
  }

  // Request a new log line
  logginator::line_t request_line(SensorData const& value) {
    using namespace logginator;
    static auto obj = request_manager().request_channel(value, logginator::ChannelDescription{ 1, "sensors" });
    return obj.request_line();
  }
}
```

### 3. Create a Manager

Implement the output backend:

```cpp
#include <iostream>

logginator::Manager_Interface & request_manager(){
  struct ConsoleOutput : logginator::Manager_Interface::Output_Interface {
    void operator()(std::string_view msg) noexcept override {
      std::cout << msg << "\n";
    }
  };

  static ConsoleOutput output;
  static logginator::Manager<std::mutex, 4096> manager{output};
  return manager;
}
```

### 4. Log Your Data

```cpp
my_app::SensorData data{
  .timestamp = 12345,
  .temperature = 22.5,
  .humidity = 65.3
};

logginator::print(data);  // Publishes automatically on scope exit
```

## Format Options

### Integer Formats
- `ascii` — Decimal string
- `hex` — Hexadecimal (0x prefix)
- `b64` — Base64 encoding

### Float Formats
- `ascii` — Shortest representation
- `ascii_fixed` — Fixed-point notation
- `ascii_scientific` — Scientific notation
- `hex` — Hexadecimal floating-point
- `b64` — Base64 encoding

### Binary Formats
- `b64` — Base64 (only option)

### String Formats
- `ascii` — Raw UTF-8
- `b64` — Base64

## Configuration

### Setup Channels

```cpp
auto& mgr = manager;

// Set ID 1 to buffer size for channel "sensors"
mgr.setup_channel(1, 1);  // Send every sample

// Set ID 2 to downsample every 10th sample
mgr.setup_channel(2, 10);

// Print channel headers
mgr.print_channels();
```

Output:
```
#01:sensors;time[ms]{ascii};temp[°C]{ascii_fixed};humidity[%]{ascii_fixed};
#02:metrics;...
```

### Downsampling

Reduces verbosity for high-frequency data:

```cpp
mgr.setup_channel(channel_id, 100);  // Only log every 100th sample
```

Counter resets per channel independently.

## Thread Safety

Logginator uses **two separate mutexes**:

- `m_mutex_buffer` — Protects the shared character buffer during serialization
- `m_mutex_list` — Protects the channel subscription list during setup

This design prevents deadlocks while ensuring thread-safe operation.

```cpp
// Safe to call from multiple threads
std::thread t1([&]{ logginator::print(data1); });
std::thread t2([&]{ logginator::print(data2); });
std::thread t3([&]{ mgr.setup_channel(1, 5); });

t1.join(); t2.join(); t3.join();
```

## Limitations & Constraints

⚠️ **No Dynamic Allocation** — Buffer sizes must be configured at compile-time  
⚠️ **Single Active Line Per Manager** — Cannot nest `request_line()` calls  
⚠️ **Buffer Overflow Throws** — Set buffers large enough for your data  
⚠️ **Not Interrupt-Safe** — Do not call from interrupt handlers  
⚠️ **ADL Dependency** — Custom types must provide print/request_line in their namespace  

## Performance Characteristics

- **Zero Runtime Overhead** for formatting decisions (all constexpr)
- **O(1)** channel lookup by ID
- **O(buffer_size)** for serialization
- **Minimal Stack Usage** — All state in static Manager instance

## Error Handling

The library throws `logginator::errors::*` exceptions:

| Exception | Cause |
|-----------|-------|
| `line_serialization_error` | Buffer overflow during formatting |
| `channel_subscription_error` | Channel ID conflict during setup |

Catch them or configure error handling in embedded environments.

## Example: Multi-Channel System

```cpp
namespace telemetry {
  struct Acceleration {
    double ax, ay, az;
  };

  struct Gyroscope {
    double gx, gy, gz;
  };

  void print(Acceleration const& a, logginator::line_t& line) {
    line.add({"ax", "m/s²", "ascii"}, a.ax);
    line.add({"ay", "m/s²", "ascii"}, a.ay);
    line.add({"az", "m/s²", "ascii"}, a.az);
  }

  logginator::line_t request_line(Acceleration const& x) {
    static auto& ch = get_manager().request_channel(x, ChannelDescription{ 10, "accel" });
    return ch.request_line();
  }

  // ...same for Gyroscope on channel 11
}

// Usage
int main() {
  auto& mgr = get_manager();
  mgr.setup_channel(request_line(Acceleration{}).get_cfg().ID, 1);   // Log all accel samples
  mgr.setup_channel(request_line(Gyroscope{}).get_cfg().ID, 10);  // Log every 10th gyro sample

  for (int i = 0; i < 1000; ++i) {
    logginator::print(accel_data[i]);      // 1000 lines
    logginator::print(gyro_data[i]);       // ~100 lines
  }
}
```

## Embedded Best Practices

1. **Pre-allocate Manager buffer** — Know your maximum line size
2. **Use downsampling** — Reduce I/O jitter
3. **Keep format functions simple** — Avoid branches, compute before logging
4. **Minimize string literals** — Use constexpr descriptions
5. **Test buffer overflow scenarios** — Ensure exception handling works

## License

MIT License — See LICENSE file for details


