#pragma once

#include <string>
#include <system_error>
#include <utility>

namespace hasten::runtime
{

enum class ErrorCode { Ok, TransportError, Timeout, Cancelled, InternalError, Unimplemented };

class HastenErrorCategory : public std::error_category
{
public:
    const char* name() const noexcept override
    {
        return "hasten";
    }

    std::string message(int ev) const override
    {
        const auto code = static_cast<ErrorCode>(ev);
        switch (code) {
            case ErrorCode::Ok:
                return "ok";
            case ErrorCode::TransportError:
                return "transport error";
            case ErrorCode::Timeout:
                return "timeout";
            case ErrorCode::Cancelled:
                return "cancelled";
            case ErrorCode::InternalError:
                return "internal error";
            case ErrorCode::Unimplemented:
                return "unimplemented";
        }
        return "unknown";
    }
};

inline const std::error_category& hasten_error_category()
{
    static HastenErrorCategory category;
    return category;
}

inline std::error_code make_error_code(ErrorCode code)
{
    return {static_cast<int>(code), hasten_error_category()};
}

struct Error {
    std::error_code code = make_error_code(ErrorCode::InternalError);
    std::string message;

    Error() = default;

    Error(ErrorCode code_, std::string message_)
        : code(make_error_code(code_))
        , message(std::move(message_))
    {
    }

    Error(std::error_code code_, std::string message_)
        : code(std::move(code_))
        , message(std::move(message_))
    {
    }
};

inline Error make_error(ErrorCode code, std::string message = {})
{
    return Error{code, std::move(message)};
}

inline Error make_error(std::error_code code, std::string message = {})
{
    return Error{std::move(code), std::move(message)};
}

inline Error unimplemented_error(std::string message)
{
    return make_error(ErrorCode::Unimplemented, std::move(message));
}

}  // namespace hasten::runtime
