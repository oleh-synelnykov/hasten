#pragma once

#include "hasten/runtime/error.hpp"
#include "hasten/runtime/result.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace hasten::runtime
{

class PayloadSink
{
public:
    virtual ~PayloadSink() = default;
    virtual result<void> append(std::span<const std::uint8_t> data) = 0;
};

class PayloadSource
{
public:
    virtual ~PayloadSource() = default;
    virtual result<std::span<const std::uint8_t>> read(std::size_t size) = 0;
    virtual bool empty() const = 0;
};

class VectorSink : public PayloadSink
{
public:
    explicit VectorSink(std::vector<std::uint8_t>& buffer)
        : buffer_(buffer)
    {
    }

    result<void> append(std::span<const std::uint8_t> data) override
    {
        buffer_.insert(buffer_.end(), data.begin(), data.end());
        return {};
    }

private:
    std::vector<std::uint8_t>& buffer_;
};

class SpanSource : public PayloadSource
{
public:
    explicit SpanSource(std::span<const std::uint8_t> data)
        : data_(data)
    {
    }

    result<std::span<const std::uint8_t>> read(std::size_t size) override
    {
        if (offset_ + size > data_.size()) {
            return unexpected_result<std::span<const std::uint8_t>>(ErrorCode::TransportError, "payload underrun");
        }
        auto view = data_.subspan(offset_, size);
        offset_ += size;
        return view;
    }

    bool empty() const override { return offset_ >= data_.size(); }

private:
    std::span<const std::uint8_t> data_;
    std::size_t offset_ = 0;
};

}  // namespace hasten::runtime
