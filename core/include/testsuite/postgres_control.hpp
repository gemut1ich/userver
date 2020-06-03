#pragma once

#include <chrono>

#include <engine/deadline.hpp>

namespace testsuite {

class PostgresControl {
 public:
  enum class ReadonlyMaster { kNotExpected, kExpected };

  PostgresControl(std::chrono::milliseconds execute_timeout,
                  std::chrono::milliseconds statement_timeout,
                  ReadonlyMaster readonly_master);

  PostgresControl();

  [[nodiscard]] engine::Deadline MakeExecuteDeadline(
      std::chrono::milliseconds duration) const;

  [[nodiscard]] std::chrono::milliseconds MakeStatementTimeout(
      std::chrono::milliseconds duration) const;

  bool IsReadonlyMasterExpected() const;

 private:
  std::chrono::milliseconds execute_timeout_;
  std::chrono::milliseconds statement_timeout_;
  bool is_readonly_master_expected_;
};

}  // namespace testsuite
