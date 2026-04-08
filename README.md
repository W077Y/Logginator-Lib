# Logginator

A lightweight, type-safe logging / telemetry framework designed for embedded systems.

The library provides a structured way to format and output measurement data with minimal runtime overhead and without dynamic memory allocation.

# Overview

logginator is built around three main concepts:

- **Manager**
  
  Central instance responsible for routing messages to an output.

- **Printer**
  
  Per-type formatter that converts structured data into a textual representation.

- **line_t**
  
  A temporary object representing a single log line. Uses RAII to ensure automatic publishing.

# Key Features

- No dynamic memory allocation

- Fixed-size buffers (std::span<char>)

- Type-safe formatting via print(value, line)

- RAII-based publishing (no manual flush required)

- Channel-based logging with IDs

- Optional per-channel downsampling

- Header generation for structured output

# Basic Usage

1) **Define your data type**

``` cpp
struct my_data
{
  std::size_t time;
  double temperature;
};
```

2) **Implement formatting**

``` cpp
void print(my_data const& value, logginator::line_t& line)
{
  line.add({ "time", "s", "int" }, value.time);
  line.add({ "temperature", "degC", "float" }, value.temperature);
}
```

3) **Provide a printer**

``` cpp
logginator::line_t request_line(my_data const&)
{
  static logginator::Printer<std::mutex, my_data, 256> printer{
    request_manager(),
    logginator::channel_description_t{ 1, "temperature" }
  };
  return printer.request_line();
}
```

4) **Log data**

``` cpp
my_data data{ .time = 42, .temperature = 23.5 };
logginator::print(data);
```


# Output Format

Each line is formatted as:

``` cpp
#<channel_id>;<value1>;<value2>;...
```

Example:

``` cpp
#01;42;23.5
```

# Header Output

Headers can be printed using:

``` cpp
manager.print_channels();
```

Example:

``` cpp
#01:temperature;time[s]{int};temperature[degC]{float};
```

# Manager Setup

You must provide an output backend:

``` cpp
struct MyOutput : logginator::Manager::Output_Interface
{
  void operator()(std::string_view msg) override
  {
    std::cout << msg;
  }
};

MyOutput output;
logginator::Manager manager{ output };
```

# Channel Configuration

## Subscribe

Handled automatically by Printer construction.

## Downsampling

``` cpp
manager.setup_channel(1, 10);
```

Only every 10th sample will be emitted.

# Design Notes

## RAII Logging

A log line is published automatically when line_t goes out of scope:

``` cpp
{
  auto line = printer.request_line();
  line.add(...);
} // <-- publish happens here
```

This prevents incomplete log lines and enforces consistent formatting.

## Customization via ADL

The library relies on Argument-Dependent Lookup (ADL):

- `print(T const&, line_t&)`
- `request_line(T const&)`

This allows extending the system for new types without modifying the library.

## No Dynamic Allocation

All buffers are statically provided:

``` cpp
logginator::Printer<T, N>
```

The size N defines the maximum length of a log line.

## Limitations

- Not thread-safe / not interrupt-safe

- Single active line per printer

- Buffer overflow handling is limited (may throw or truncate depending on type)

- Requires careful configuration in embedded environments (exceptions, RTTI, etc.)

## Recommendations

- Use sufficiently large buffers per channel

- Avoid logging in interrupt context

- Keep formatting functions simple and deterministic

- Ensure channel IDs are unique

## Example

``` cpp
for (std::size_t i = 0; i < 10; ++i)
{
  my_data data{ i, 20.0 + i };
  logginator::print(data);
}
```


# License

This project is licensed under the MIT License.


