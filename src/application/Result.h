#pragma once

#include <optional>
#include <utility>
#include <variant>

#include "application/Error.h"

// application 層。
//
// Result<T>：成功時帶一個 T，失敗時帶一個 Error。C++20 沒有 std::expected，所以自己放一個最小版。
//   Result<int> r = 42;            // 成功
//   Result<int> r = makeError(...) // 失敗
//   if (r) use(r.value()); else handle(r.error());
// 在失敗的 Result 上呼叫 value()（或在成功的上面呼叫 error()）是呼叫端的程式錯誤，會丟出 std::bad_variant_access。
// 這是少數允許的例外：它不是 port 的執行期失敗，而是寫錯了程式。
namespace aoi {

template <typename T>
class Result {
public:
    Result(T value) : v_(std::move(value)) {}          // NOLINT(google-explicit-constructor)：成功可以直接回傳值
    Result(Error error) : v_(std::move(error)) {}      // NOLINT(google-explicit-constructor)：失敗可以直接回傳 Error

    bool ok() const noexcept { return v_.index() == 0; }
    explicit operator bool() const noexcept { return ok(); }

    T& value() & { return std::get<0>(v_); }
    const T& value() const& { return std::get<0>(v_); }
    T&& value() && { return std::get<0>(std::move(v_)); }

    const Error& error() const { return std::get<1>(v_); }

private:
    std::variant<T, Error> v_;
};

template <>
class Result<void> {
public:
    Result() = default;                                  // 成功
    Result(Error error) : e_(std::move(error)) {}        // NOLINT(google-explicit-constructor)

    static Result success() { return Result(); }

    bool ok() const noexcept { return !e_.has_value(); }
    explicit operator bool() const noexcept { return ok(); }
    const Error& error() const { return e_.value(); }

private:
    std::optional<Error> e_;
};

}  // namespace aoi
