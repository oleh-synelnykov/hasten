#pragma once

#include "hasten/runtime/encoding.hpp"
#include "hasten/runtime/result.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace hasten::runtime::rpc
{

enum class Status : std::uint8_t {
    Ok = 0,
    ApplicationError = 1,
    InvalidRequest = 2,
    NotFound = 3,
    InternalError = 4,
};

struct Request {
    std::uint64_t module_id = 0;
    std::uint64_t interface_id = 0;
    std::uint64_t method_id = 0;
    Encoding encoding = Encoding::Hb1;
    std::vector<std::uint8_t> payload_storage;
    std::span<const std::uint8_t> payload;
};

struct Response {
    Status status = Status::Ok;
    std::vector<std::uint8_t> body;
};

using Responder = std::function<void(Response)>;
using Handler = std::function<void(std::shared_ptr<Request>, Responder)>;

void register_handler(std::uint64_t interface_id, Handler handler);
std::optional<Handler> find_handler(std::uint64_t interface_id);

}  // namespace hasten::runtime::rpc
