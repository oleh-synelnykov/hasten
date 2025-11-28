#pragma once

#include "hasten/runtime/encoding.hpp"
#include "hasten/runtime/result.hpp"

#include <cstdint>
#include <vector>

namespace hasten::runtime
{

struct Frame {
    std::uint32_t type = 0;
    std::uint32_t flags = 0;
    std::uint64_t stream_id = 0;
    std::vector<std::uint8_t> payload;
};

class Channel
{
public:
    virtual ~Channel() = default;
    virtual Encoding encoding() const = 0;
    virtual Result<void> send(Frame frame) = 0;
};

class Dispatcher
{
public:
    virtual ~Dispatcher() = default;
    virtual std::uint64_t open_stream() = 0;
    virtual void close_stream(std::uint64_t stream_id) = 0;
};

}  // namespace hasten::runtime
