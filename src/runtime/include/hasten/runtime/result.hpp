#pragma once

#include "hasten/runtime/error.hpp"

#include <expected>
#include <string>
#include <system_error>
#include <utility>

namespace hasten::runtime
{

template <typename T>
using Result = std::expected<T, Error>;

template <typename T = void>
inline Result<T> unexpected_result(Error error)
{
    return std::unexpected(std::move(error));
}

template <typename T = void>
inline Result<T> unexpected_result(ErrorCode code, std::string message = {})
{
    return unexpected_result<T>(make_error(code, std::move(message)));
}

template <typename T = void>
inline Result<T> unexpected_result(std::error_code code, std::string message = {})
{
    return unexpected_result<T>(make_error(std::move(code), std::move(message)));
}

template <typename T = void>
inline Result<T> unimplemented_result(std::string message)
{
    return unexpected_result<T>(unimplemented_error(std::move(message)));
}

}  // namespace hasten::runtime
