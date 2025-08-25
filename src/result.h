#pragma once

#include <optional>
#include <utility>
#include "error.h"

namespace okj {

template <typename T>
class Result {
public:
    static Result ok(T value) {
        Result r;
        r.m_value = std::move(value);
        return r;
    }

    static Result err(Error error) {
        Result r;
        r.m_error = std::move(error);
        return r;
    }

    bool has_value() const { return m_error.code == ErrorCode::None; }
    const T &value() const { return *m_value; }
    const Error &error() const { return m_error; }

private:
    std::optional<T> m_value;
    Error m_error;
};

} // namespace okj

