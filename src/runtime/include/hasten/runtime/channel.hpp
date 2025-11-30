#pragma once

#include "hasten/runtime/encoding.hpp"
#include "hasten/runtime/frame.hpp"
#include "hasten/runtime/result.hpp"
#include "hasten/runtime/rpc.hpp"

#include <optional>

namespace hasten::runtime
{

class Channel
{
public:
    virtual ~Channel() = default;
    virtual Encoding encoding() const = 0;
    virtual Result<void> send(Frame frame) = 0;
    virtual Result<Frame> receive() = 0;
    virtual void close() = 0;
};

class Dispatcher
{
public:
    virtual ~Dispatcher() = default;
    virtual std::uint64_t open_stream() = 0;
    virtual void close_stream(std::uint64_t stream_id) = 0;
    virtual void set_response_handler(std::uint64_t stream_id, rpc::Responder handler) = 0;
    virtual std::optional<rpc::Responder> take_response_handler(std::uint64_t stream_id) = 0;
};

}  // namespace hasten::runtime
