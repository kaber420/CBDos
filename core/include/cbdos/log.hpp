#pragma once

#include <cstdio>

namespace cbdos {
namespace log {

enum class LogLevel {
    Error,
    Warn,
    Info,
    Debug,
    Verbose
};

void write(LogLevel level, const char* tag, const char* format, ...);

} // namespace log
} // namespace cbdos

#define CBD_LOG_E(tag, format, ...) cbdos::log::write(cbdos::log::LogLevel::Error, tag, format, ##__VA_ARGS__)
#define CBD_LOG_W(tag, format, ...) cbdos::log::write(cbdos::log::LogLevel::Warn, tag, format, ##__VA_ARGS__)
#define CBD_LOG_I(tag, format, ...) cbdos::log::write(cbdos::log::LogLevel::Info, tag, format, ##__VA_ARGS__)
#define CBD_LOG_D(tag, format, ...) cbdos::log::write(cbdos::log::LogLevel::Debug, tag, format, ##__VA_ARGS__)
#define CBD_LOG_V(tag, format, ...) cbdos::log::write(cbdos::log::LogLevel::Verbose, tag, format, ##__VA_ARGS__)

