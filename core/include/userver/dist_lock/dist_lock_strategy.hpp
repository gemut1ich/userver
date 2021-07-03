#pragma once

/// @file dist_lock/dist_lock_strategy.hpp
/// @brief @copybrief dist_lock::DistLockStrategyBase

#include <chrono>
#include <exception>
#include <string>

namespace dist_lock {

/// Indicates that lock cannot be acquired because it's busy.
class LockIsAcquiredByAnotherHostException : public std::exception {};

/// Interface for distributed lock strategies
class DistLockStrategyBase {
 public:
  virtual ~DistLockStrategyBase() = default;

  /// Acquires the distributed lock.
  ///
  /// @param lock_ttl The duration for which the lock must be held.
  /// @param locker_id Globally unique ID of the locking entity.
  /// @throws LockIsAcqiredByAnotherHostError when the lock is busy
  /// @throws anything else when the locking fails, strategy is responsible for
  /// cleanup, Release won't be invoked.
  virtual void Acquire(std::chrono::milliseconds lock_ttl,
                       const std::string& locker_id) = 0;

  /// Releases the lock.
  ///
  /// @param locker_id Globally unique ID of the locking entity, must be the
  /// same as in Acquire().
  /// @note Exceptions are ignored.
  virtual void Release(const std::string& locker_id) = 0;
};

}  // namespace dist_lock