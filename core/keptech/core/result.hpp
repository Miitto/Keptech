#pragma once

#include "keptech/core/kt-logger.hpp"

namespace kt {
  template <typename T, typename E, const E E_OK>
    requires std::is_same_v<decltype(E_OK), E>
  class Result {
  public:
    Result(const T& value)
      requires std::is_constructible_v<T, const T&>
        : m_value(value), m_error(E_OK) {}
    Result(T&& value)
      requires std::is_constructible_v<T, T&&>
        : m_value(std::move(value)), m_error(E_OK) {}
    Result(E error) : m_error(error) {}

    [[nodiscard]]
    bool isOk() const {
      return m_error == E_OK;
    }
    [[nodiscard]]
    bool isError() const {
      return !isOk();
    }
    operator bool() const { return isOk(); }

    [[nodiscard]]
    T& value() {
      if (isError()) {
        KT_ABORT("Attempted to access value of an error result: {}", static_cast<int>(m_error));
      }
      return m_value;
    }

    [[nodiscard]]
    const T& value() const {
      if (isError()) {
        KT_ABORT("Attempted to access value of an error result: {}", static_cast<int>(m_error));
      }
      return m_value;
    }

    [[nodiscard]]
    E error() const {
      return m_error;
    }

    template <typename U, typename F> Result<U, E, E_OK> map(F func) const {
      if (isOk()) {
        return Result<U, E, E_OK>(func(m_value));
      } else {
        return Result<U, E, E_OK>(m_error);
      }
    }

    template <typename F> Result<T, E, E_OK> mapError(F func) const {
      if (isError()) {
        return Result<T, E, E_OK>(func(m_error));
      } else {
        return *this;
      }
    }

    Result(const Result&) = default;
    Result& operator=(const Result&) = default;
    Result(Result&& other) noexcept : m_value(std::move(other.m_value)), m_error(other.m_error) {}
    Result& operator=(Result&& other) noexcept {
      if (this != &other) {
        m_value = std::move(other.m_value);
        m_error = other.m_error;
      }
      return *this;
    }

    ~Result() {
      if (isOk()) {
        m_value.~T();
      }
    }

  private:
    union {
      T m_value;
      void* m_dummy = nullptr;
    };
    E m_error;
  };
} // namespace kt