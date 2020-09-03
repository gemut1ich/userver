#pragma once

/// @file logging/log.hpp
/// @brief Logging helpers

#include <chrono>
#include <exception>
#include <iterator>
#include <memory>
#include <optional>
#include <ostream>
#include <string_view>

#include <boost/system/error_code.hpp>

#include <logging/level.hpp>
#include <logging/log_extra.hpp>
#include <logging/logger.hpp>
#include <utils/encoding/hex.hpp>
#include <utils/encoding/tskv.hpp>
#include <utils/meta.hpp>

namespace logging {

namespace impl {

struct Noop {};

struct HexBase {
  uint64_t value;

  template <typename Unsigned,
            typename = std::enable_if_t<std::is_unsigned_v<Unsigned>>>
  explicit HexBase(Unsigned value) : value(value) {
    static_assert(sizeof(Unsigned) <= sizeof(value));
  }

  template <typename T>
  explicit HexBase(T* pointer) : HexBase(reinterpret_cast<uintptr_t>(pointer)) {
    static_assert(sizeof(value) == sizeof(uintptr_t));
  }
};

}  // namespace impl

/// Formats value in a hex mode with the fixed length representation.
struct Hex final : impl::HexBase {
  using impl::HexBase::HexBase;
};

/// Formats value in a hex mode with the shortest representation, just like
/// std::to_chars does.
struct HexShort final : impl::HexBase {
  using impl::HexBase::HexBase;
};

/// Returns default logger
LoggerPtr DefaultLogger();

/// Atomically replaces default logger and returns the old one
LoggerPtr SetDefaultLogger(LoggerPtr);

/// Sets new log level for default logger
void SetDefaultLoggerLevel(Level);

/// Returns log level for default logger
Level GetDefaultLoggerLevel();

/// Stream-like tskv-formatted log message builder.
///
/// Users can add LogHelper& operator<<(LogHelper&, ) overloads to use a faster
/// localeless logging, rather than outputting data through the ostream
/// operator.
class LogHelper final {
 public:
  enum class Mode { kDefault, kNoSpan };

  /// @brief Constructs LogHelper with span logging
  /// @param level message log level
  /// @param path path of the source file that generated the message
  /// @param line line of the source file that generated the message
  /// @param func name of the function that generated the message
  /// @param mode logging mode - with or without span
  LogHelper(LoggerPtr logger, Level level, const char* path, int line,
            const char* func, Mode mode = Mode::kDefault) noexcept;

  ~LogHelper();

  LogHelper(LogHelper&&) = delete;
  LogHelper(const LogHelper&) = delete;
  LogHelper& operator=(LogHelper&&) = delete;
  LogHelper& operator=(const LogHelper&) = delete;

  // Helper function that could be called on LogHelper&& to get LogHelper&.
  LogHelper& AsLvalue() noexcept { return *this; }

  bool IsLimitReached() const;

  template <typename T>
  LogHelper& operator<<(const T& value) {
    constexpr bool encode = utils::encoding::TypeNeedsEncodeTskv<T>::value;
    EncodingGuard guard{*this, encode ? Encode::kValue : Encode::kNone};

    if constexpr (std::is_same_v<T, char>) {
      Put(value);
    } else if constexpr (std::is_constructible_v<std::string_view, T>) {
      Put(value);
    } else if constexpr (std::is_floating_point_v<T>) {
      PutFloatingPoint(value);
    } else if constexpr (std::is_signed_v<T>) {
      PutSigned(value);
    } else if constexpr (std::is_unsigned_v<T>) {  // `bool` falls here
      PutUnsigned(value);
    } else if constexpr (std::is_base_of_v<std::exception, T>) {
      PutException(value);
    } else if constexpr (meta::kIsOstreamWritable<T>) {
      Stream() << value;
    } else if constexpr (meta::kIsRange<T>) {
      PutRange(value);
    } else {
      static_assert(!sizeof(T),
                    "Please implement logging for your type: "
                    "LogHelper& operator<<(LogHelper& lh, const T& value)");
    }

    return *this;
  }

  /// Extends internal LogExtra
  LogHelper& operator<<(const LogExtra& extra);

  /// Extends internal LogExtra
  LogHelper& operator<<(LogExtra&& extra);

  LogHelper& operator<<(Hex hex);

  LogHelper& operator<<(HexShort hex);

  /// @cond
  // For internal use only!
  operator impl::Noop() const noexcept { return {}; }
  /// @endcond

 private:
  enum class Encode { kNone, kValue, kKeyReplacePeriod };

  struct EncodingGuard {
    LogHelper& lh;

    EncodingGuard(LogHelper& lh, Encode mode) noexcept;
    ~EncodingGuard();
  };

  void DoLog() noexcept;

  void AppendLogExtra();
  void LogTextKey();
  void LogModule(const char* path, int line, const char* func);
  void LogIds();
  void LogSpan();

  void PutFloatingPoint(float value);
  void PutFloatingPoint(double value);
  void PutFloatingPoint(long double value);
  void PutUnsigned(unsigned long long value);
  void PutSigned(long long value);
  void Put(std::string_view value);
  void Put(char value);
  void PutException(const std::exception& ex);

  template <typename T>
  void PutRange(const T& range);

  std::ostream& Stream();

  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

inline LogHelper& operator<<(LogHelper& lh, std::error_code ec) {
  lh << ec.category().name() << ':' << ec.value() << " (" << ec.message()
     << ')';
  return lh;
}

template <typename T>
LogHelper& operator<<(LogHelper& lh, const std::atomic<T>& value) {
  return lh << value.load();
}

template <typename T>
LogHelper& operator<<(LogHelper& lh, const T* value) {
  if (value == nullptr) {
    lh << "(null)";
  } else if constexpr (std::is_same_v<T, char>) {
    lh << std::string_view(value);
  } else {
    lh << Hex{value};
  }
  return lh;
}

template <typename T>
LogHelper& operator<<(LogHelper& lh, T* value) {
  return lh << static_cast<const T*>(value);
}

inline LogHelper& operator<<(LogHelper& lh, boost::system::error_code ec) {
  lh << ec.category().name() << ':' << ec.value();
  return lh;
}

template <typename T>
LogHelper& operator<<(LogHelper& lh, const std::optional<T>& value) {
  if (value)
    lh << *value;
  else
    lh << "(none)";
  return lh;
}

template <class Result, class... Args>
LogHelper& operator<<(LogHelper& lh, Result (*)(Args...)) {
  static_assert(!sizeof(Result),
                "Outputting functions or std::ostream formatters is forbidden");
  return lh;
}

LogHelper& operator<<(LogHelper& lh, std::chrono::system_clock::time_point tp);

template <typename T, typename U>
LogHelper& operator<<(LogHelper& lh, const std::pair<T, U>& pair) {
  lh << pair.first << ": " << pair.second;
  return lh;
}

template <typename T>
void LogHelper::PutRange(const T& range) {
  static_assert(meta::kIsRange<T>);
  constexpr std::string_view kSeparator = ", ";
  Put('[');

  bool is_first = true;
  auto curr = std::begin(range);
  const auto end = std::end(range);

  while (curr != end) {
    if (IsLimitReached()) {
      break;
    }
    if (is_first) {
      is_first = false;
    } else {
      Put(kSeparator);
    }
    *this << *curr;
    ++curr;
  }

  const auto extra_elements = std::distance(curr, end);

  if (extra_elements != 0) {
    if (!is_first) {
      *this << ' ';
    }
    *this << "...(" << extra_elements << " more)";
  }

  Put(']');
}

/// Forces flush of default logger message queue
void LogFlush();

}  // namespace logging

/// @brief Builds a stream and evaluates a message for the logger.
/// @hideinitializer
#define DO_LOG_TO(logger, lvl) \
  ::logging::LogHelper(logger, lvl, __FILE__, __LINE__, __func__).AsLvalue()

// static_cast<int> below are workarounds for clangs -Wtautological-compare

/// @brief If lvl matches the verbosity then builds a stream and evaluates a
/// message for the logger.
/// @hideinitializer
#define LOG_TO(logger, lvl)                                              \
  __builtin_expect(                                                      \
      !::logging::ShouldLog(lvl),                                        \
      static_cast<int>(lvl) < static_cast<int>(::logging::Level::kInfo)) \
      ? ::logging::impl::Noop{}                                          \
      : DO_LOG_TO(logger, lvl)

/// @brief If lvl matches the verbosity then builds a stream and evaluates a
/// message for the default logger.
#define LOG(lvl) LOG_TO(::logging::DefaultLogger(), lvl)

#define LOG_TRACE() LOG(::logging::Level::kTrace)
#define LOG_DEBUG() LOG(::logging::Level::kDebug)
#define LOG_INFO() LOG(::logging::Level::kInfo)
#define LOG_WARNING() LOG(::logging::Level::kWarning)
#define LOG_ERROR() LOG(::logging::Level::kError)
#define LOG_CRITICAL() LOG(::logging::Level::kCritical)

#define LOG_TRACE_TO(logger) LOG_TO(logger, ::logging::Level::kTrace)
#define LOG_DEBUG_TO(logger) LOG_TO(logger, ::logging::Level::kDebug)
#define LOG_INFO_TO(logger) LOG_TO(logger, ::logging::Level::kInfo)
#define LOG_WARNING_TO(logger) LOG_TO(logger, ::logging::Level::kWarning)
#define LOG_ERROR_TO(logger) LOG_TO(logger, ::logging::Level::kError)
#define LOG_CRITICAL_TO(logger) LOG_TO(logger, ::logging::Level::kCritical)
