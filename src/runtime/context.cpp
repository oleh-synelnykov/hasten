#include "hasten/runtime/context.hpp"

#include "hasten/runtime/channel.hpp"
#include "hasten/runtime/frame.hpp"
#include "hasten/runtime/rpc.hpp"
#include "hasten/runtime/uds.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace hasten::runtime
{
namespace
{

Result<std::uint64_t> read_varint(const std::vector<std::uint8_t>& buffer, std::size_t& offset)
{
    std::uint64_t result = 0;
    int shift = 0;
    while (offset < buffer.size()) {
        std::uint8_t byte = buffer[offset++];
        result |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
            return result;
        }
        shift += 7;
        if (shift >= 64) {
            return unexpected_result<std::uint64_t>(ErrorCode::TransportError, "varint too long");
        }
    }
    return unexpected_result<std::uint64_t>(ErrorCode::TransportError, "truncated varint");
}

struct ParsedRequest {
    rpc::Request request;
    std::uint64_t request_id = 0;
};

Result<ParsedRequest> parse_rpc_request(const std::vector<std::uint8_t>& payload)
{
    std::size_t offset = 0;
    auto module_id = read_varint(payload, offset);
    if (!module_id) {
        return std::unexpected(module_id.error());
    }
    auto interface_id = read_varint(payload, offset);
    if (!interface_id) {
        return std::unexpected(interface_id.error());
    }
    auto method_id = read_varint(payload, offset);
    if (!method_id) {
        return std::unexpected(method_id.error());
    }
    auto encoding_id = read_varint(payload, offset);
    if (!encoding_id) {
        return std::unexpected(encoding_id.error());
    }
    Encoding encoding = Encoding::Hb1;
    if (*encoding_id == static_cast<std::uint64_t>(Encoding::Hb1)) {
        encoding = Encoding::Hb1;
    } else {
        return unexpected_result<ParsedRequest>(ErrorCode::TransportError, "unsupported encoding");
    }
    auto request_id = read_varint(payload, offset);
    if (!request_id) {
        return std::unexpected(request_id.error());
    }

    rpc::Request req;
    req.module_id = *module_id;
    req.interface_id = *interface_id;
    req.method_id = *method_id;
    req.encoding = encoding;
    if (offset <= payload.size()) {
        req.payload_storage.assign(payload.begin() + static_cast<std::ptrdiff_t>(offset), payload.end());
        req.payload = req.payload_storage;
    }

    ParsedRequest parsed;
    parsed.request = std::move(req);
    parsed.request_id = *request_id;
    return parsed;
}

std::vector<std::uint8_t> build_response_payload(rpc::Status status,
                                                 std::span<const std::uint8_t> body)
{
    std::vector<std::uint8_t> payload;
    auto append_varint = [&payload](std::uint64_t value) {
        while (value >= 0x80) {
            payload.push_back(static_cast<std::uint8_t>(value | 0x80));
            value >>= 7;
        }
        payload.push_back(static_cast<std::uint8_t>(value));
    };
    append_varint(static_cast<std::uint64_t>(Encoding::Hb1));
    payload.push_back(static_cast<std::uint8_t>(status));
    payload.insert(payload.end(), body.begin(), body.end());
    return payload;
}

Result<rpc::Response> parse_rpc_response(const std::vector<std::uint8_t>& payload)
{
    std::size_t offset = 0;
    auto encoding_id = read_varint(payload, offset);
    if (!encoding_id) {
        return std::unexpected(encoding_id.error());
    }
    Encoding encoding = Encoding::Hb1;
    if (*encoding_id != static_cast<std::uint64_t>(Encoding::Hb1)) {
        return unexpected_result<rpc::Response>(ErrorCode::TransportError, "unsupported encoding");
    }
    if (offset >= payload.size()) {
        return unexpected_result<rpc::Response>(ErrorCode::TransportError, "missing response status");
    }
    rpc::Response response;
    response.status = static_cast<rpc::Status>(payload[offset++]);
    response.body.assign(payload.begin() + static_cast<std::ptrdiff_t>(offset), payload.end());
    return response;
}

class Session : public std::enable_shared_from_this<Session>
{
public:
    enum class Kind { Client, Server };
    using FrameCallback = std::function<void(std::shared_ptr<Session>, Frame&&)>;
    using ErrorCallback = std::function<void(std::shared_ptr<Session>, Error)>;

    Session(std::uint64_t id,
            std::shared_ptr<Channel> channel,
            std::shared_ptr<Dispatcher> dispatcher,
            std::shared_ptr<Executor> exec,
            Kind kind,
            FrameCallback frame_cb,
            ErrorCallback error_cb)
        : id_(id)
        , channel_(std::move(channel))
        , dispatcher_(std::move(dispatcher))
        , executor_(std::move(exec))
        , kind_(kind)
        , frame_callback_(std::move(frame_cb))
        , error_callback_(std::move(error_cb))
    {
    }

    ~Session()
    {
        stop();
    }

    void start()
    {
        bool expected = false;
        if (!running_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return;
        }
        io_thread_ = std::thread([self = shared_from_this()] { self->io_loop(); });
    }

    void stop()
    {
        if (!running_.exchange(false, std::memory_order_acq_rel)) {
            return;
        }
        if (channel_) {
            channel_->close();
        }
        if (io_thread_.joinable()) {
            if (std::this_thread::get_id() == io_thread_.get_id()) {
                io_thread_.detach();
            } else {
                io_thread_.join();
            }
        }
    }

    Result<void> send(Frame frame)
    {
        if (!channel_) {
            return unexpected_result(ErrorCode::TransportError, "Channel closed");
        }
        return channel_->send(std::move(frame));
    }

    std::uint64_t id() const { return id_; }
    Kind kind() const { return kind_; }

    Encoding peer_encoding() const { return peer_encoding_.load(std::memory_order_relaxed); }
    void set_peer_encoding(Encoding encoding) { peer_encoding_.store(encoding, std::memory_order_relaxed); }

    std::uint64_t open_stream() { return dispatcher_ ? dispatcher_->open_stream() : 0; }
    void close_stream(std::uint64_t stream_id)
    {
        if (dispatcher_) {
            dispatcher_->close_stream(stream_id);
        }
    }

private:
    void io_loop()
    {
        while (running_.load(std::memory_order_relaxed)) {
            auto frame = channel_->receive();
            if (!frame) {
                if (error_callback_) {
                    error_callback_(shared_from_this(), frame.error());
                }
                break;
            }
            frame_callback_(shared_from_this(), std::move(*frame));
        }
    }

    std::uint64_t id_ = 0;
    std::shared_ptr<Channel> channel_;
    std::shared_ptr<Dispatcher> dispatcher_;
    std::shared_ptr<Executor> executor_;
    Kind kind_ = Kind::Client;
    FrameCallback frame_callback_;
    ErrorCallback error_callback_;
    std::thread io_thread_;
    std::atomic<bool> running_{false};
    std::atomic<Encoding> peer_encoding_{Encoding::Hb1};
};

struct QueuedFrame {
    std::shared_ptr<Session> session;
    Frame frame;
};

struct ListenerState {
    std::shared_ptr<uds::Server> server;
    std::string path;
    std::thread thread;
    std::atomic<bool> running{true};
};

void log_warning(const char* fmt, const std::string& path, const Error& error)
{
    std::fprintf(stderr, "hasten runtime: ");
    std::fprintf(stderr, fmt, path.c_str());
    std::fprintf(stderr, ": %s\n", error.message.c_str());
}

}  // namespace

class Context::Impl
{
public:
    explicit Impl(ContextConfig config)
        : config_(config)
        , dispatcher_(uds::make_dispatcher())
        , executor_(make_default_executor())
    {
        if (!config_.worker_threads) {
            config_.worker_threads = std::max<std::size_t>(1, std::thread::hardware_concurrency());
        }
    }

    ~Impl()
    {
        stop();
        join();
    }

    Result<void> listen(const std::string& path)
    {
        auto server = uds::listen(path);
        if (!server) {
            return std::unexpected(server.error());
        }

        auto state = std::make_shared<ListenerState>();
        state->server = *server;
        state->path = path;
        state->thread = std::thread([this, state] { accept_loop(state); });

        {
            std::lock_guard lock(listeners_mutex_);
            listeners_.push_back(state);
        }
        return {};
    }

    Result<void> connect(const std::string& path)
    {
        auto channel = uds::connect(path);
        if (!channel) {
            return std::unexpected(channel.error());
        }
        return add_session(*channel, Session::Kind::Client);
    }

    Result<void> attach_channel(std::shared_ptr<Channel> channel, bool server_side)
    {
        return add_session(std::move(channel), server_side ? Session::Kind::Server : Session::Kind::Client);
    }

    void set_executor(std::shared_ptr<Executor> exec)
    {
        executor_ = std::move(exec);
        if (!executor_) {
            executor_ = make_default_executor();
        }
    }

    void start()
    {
        if (!config_.managed_reactor) {
            return;
        }
        bool expected = false;
        if (reactor_running_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            reactor_thread_ = std::thread([this] { run(); });
        }
    }

    void stop()
    {
        bool expected = false;
        if (!stop_requested_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return;
        }
        queue_cv_.notify_all();

        {
            std::lock_guard lock(listeners_mutex_);
            for (auto& state : listeners_) {
                state->running = false;
                if (state->server) {
                    state->server->close();
                }
            }
        }

        {
            std::vector<std::shared_ptr<Session>> sessions_snapshot;
            {
                std::lock_guard lock(sessions_mutex_);
                sessions_snapshot = sessions_;
                sessions_.clear();
            }
            for (auto& session : sessions_snapshot) {
                session->stop();
            }
        }
    }

    void join()
    {
        if (reactor_thread_.joinable()) {
            reactor_thread_.join();
        }
        {
            std::lock_guard lock(listeners_mutex_);
            for (auto& state : listeners_) {
                if (state->thread.joinable()) {
                    state->thread.join();
                }
            }
            listeners_.clear();
        }
        reactor_running_.store(false, std::memory_order_release);
    }

    std::size_t run()
    {
        return run_loop(/*block=*/true, /*single=*/false);
    }

    std::size_t run_one()
    {
        return run_loop(/*block=*/true, /*single=*/true);
    }

    std::size_t poll()
    {
        return run_loop(/*block=*/false, /*single=*/false);
    }

private:
    Result<void> add_session(std::shared_ptr<Channel> channel, Session::Kind kind)
    {
        auto session = std::make_shared<Session>(
            next_session_id_++,
            std::move(channel),
            dispatcher_,
            executor_,
            kind,
            [this](std::shared_ptr<Session> session, Frame&& frame) { enqueue_frame(std::move(session), std::move(frame)); },
            [this](std::shared_ptr<Session> session, Error error) { handle_session_error(std::move(session), std::move(error)); });

        {
            std::lock_guard lock(sessions_mutex_);
            sessions_.push_back(session);
        }

        session->start();
        send_initial_settings(session);
        return {};
    }

    void accept_loop(std::shared_ptr<ListenerState> state)
    {
        while (state->running.load(std::memory_order_relaxed) && !stop_requested_.load(std::memory_order_relaxed)) {
            auto channel = state->server->accept();
            if (!channel) {
                if (!state->running.load(std::memory_order_relaxed) || stop_requested_.load(std::memory_order_relaxed)) {
                    break;
                }
                log_warning("accept failed on %s", state->path, channel.error());
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }
            if (auto res = add_session(*channel, Session::Kind::Server); !res) {
                log_warning("session creation failed on %s", state->path, res.error());
            }
        }
    }

    void enqueue_frame(std::shared_ptr<Session> session, Frame frame)
    {
        {
            std::lock_guard lock(queue_mutex_);
            frame_queue_.push_back(QueuedFrame{std::move(session), std::move(frame)});
        }
        queue_cv_.notify_one();
    }

    std::optional<QueuedFrame> pop_frame(bool block)
    {
        std::unique_lock lock(queue_mutex_);
        if (block) {
            queue_cv_.wait(lock, [this] {
                return stop_requested_.load(std::memory_order_relaxed) || !frame_queue_.empty();
            });
        }

        if (frame_queue_.empty()) {
            return std::nullopt;
        }

        auto item = std::move(frame_queue_.front());
        frame_queue_.pop_front();
        return item;
    }

    std::size_t run_loop(bool block, bool single)
    {
        std::size_t processed = 0;
        const bool allow_block = block;
        while (true) {
            auto item = pop_frame(block);
            if (!item) {
                if (block) {
                    if (stop_requested_.load(std::memory_order_relaxed)) {
                        break;
                    }
                    continue;
                }
                break;
            }
            handle_frame(item->session, std::move(item->frame));
            ++processed;

            if (single) {
                break;
            }
            block = allow_block;
        }
        return processed;
    }

    void handle_session_error(std::shared_ptr<Session> session, Error error)
    {
        std::fprintf(stderr,
                     "hasten runtime: session %" PRIu64 " error: %s\n",
                     static_cast<std::uint64_t>(session->id()),
                     error.message.c_str());
        remove_session(session);
    }

    void send_initial_settings(const std::shared_ptr<Session>& session)
    {
        Frame frame;
        frame.header.type = FrameType::Settings;
        frame.payload.push_back(static_cast<std::uint8_t>(Encoding::Hb1));
        if (auto res = session->send(std::move(frame)); !res) {
            handle_session_error(session, res.error());
        }
    }

    void handle_frame(const std::shared_ptr<Session>& session, Frame frame)
    {
        switch (frame.header.type) {
            case FrameType::Ping:
                handle_ping(session, std::move(frame));
                break;
            case FrameType::Settings:
                handle_settings(session, frame);
                break;
            case FrameType::Goodbye:
                handle_goodbye(session, frame);
                break;
            case FrameType::Cancel:
                handle_cancel(session, frame);
                break;
            case FrameType::Error:
                handle_error(session, frame);
                break;
            case FrameType::Data:
                if (session->kind() == Session::Kind::Server) {
                    handle_server_data(session, std::move(frame));
                } else {
                    handle_client_data(session, std::move(frame));
                }
                break;
        }
    }

    void handle_ping(const std::shared_ptr<Session>& session, Frame frame)
    {
        Frame response;
        response.header.type = FrameType::Ping;
        response.header.flags = frame.header.flags;
        response.header.stream_id = frame.header.stream_id;
        response.payload = frame.payload;
        if (auto res = session->send(std::move(response)); !res) {
            handle_session_error(session, res.error());
        }
    }

    void handle_settings(const std::shared_ptr<Session>& session, const Frame& frame)
    {
        if (!frame.payload.empty()) {
            auto encoding = static_cast<Encoding>(frame.payload.front());
            session->set_peer_encoding(encoding);
        }
    }

    void handle_goodbye(const std::shared_ptr<Session>& session, const Frame&)
    {
        std::fprintf(stderr,
                     "hasten runtime: peer requested GOODBYE for session %" PRIu64 "\n",
                     static_cast<std::uint64_t>(session->id()));
        session->stop();
        remove_session(session);
    }

    void handle_cancel(const std::shared_ptr<Session>&, const Frame& frame)
    {
        std::fprintf(stderr,
                     "hasten runtime: cancel frame for stream %" PRIu64 " ignored (not implemented)\n",
                     static_cast<std::uint64_t>(frame.header.stream_id));
    }

    void handle_error(const std::shared_ptr<Session>& session, const Frame& frame)
    {
        std::fprintf(stderr,
                     "hasten runtime: error frame from session %" PRIu64 " (%zu bytes payload)\n",
                     static_cast<std::uint64_t>(session->id()),
                     frame.payload.size());
    }

    void handle_server_data(const std::shared_ptr<Session>& session, Frame frame)
    {
        auto parsed = parse_rpc_request(frame.payload);
        if (!parsed) {
            send_rpc_response(session, frame.header.stream_id, rpc::Response{rpc::Status::InvalidRequest, {}});
            return;
        }

        auto handler = rpc::find_handler(parsed->request.interface_id);
        if (!handler) {
            send_rpc_response(session, frame.header.stream_id, rpc::Response{rpc::Status::NotFound, {}});
            return;
        }

        auto request_ptr = std::make_shared<rpc::Request>(std::move(parsed->request));
        auto weak_session = std::weak_ptr<Session>(session);
        auto stream_id = frame.header.stream_id;

        auto responder = [weak_session, stream_id, this](rpc::Response response) {
            if (auto locked = weak_session.lock()) {
                send_rpc_response(locked, stream_id, std::move(response));
            }
        };

        (*handler)(std::move(request_ptr), std::move(responder));
    }

    void handle_client_data(const std::shared_ptr<Session>&, Frame frame)
    {
        auto response = parse_rpc_response(frame.payload);
        if (!response) {
            if (dispatcher_) {
                dispatcher_->close_stream(frame.header.stream_id);
            }
            std::fprintf(stderr,
                         "hasten runtime: failed to decode response for stream %" PRIu64 ": %s\n",
                         static_cast<std::uint64_t>(frame.header.stream_id),
                         response.error().message.c_str());
            return;
        }

        if (!dispatcher_) {
            return;
        }

        auto handler = dispatcher_->take_response_handler(frame.header.stream_id);
        if (!handler) {
            std::fprintf(stderr,
                         "hasten runtime: no response handler for stream %" PRIu64 "\n",
                         static_cast<std::uint64_t>(frame.header.stream_id));
            return;
        }

        auto cb = std::move(*handler);
        auto resp = std::move(*response);
        if (executor_) {
            executor_->schedule([cb = std::move(cb), resp = std::move(resp)]() mutable {
                cb(std::move(resp));
            });
        } else {
            cb(std::move(resp));
        }
    }

    void send_rpc_response(const std::shared_ptr<Session>& session,
                           std::uint64_t stream_id,
                           rpc::Response response)
    {
        Frame reply;
        reply.header.type = FrameType::Data;
        reply.header.flags = FrameFlagEndStream;
        reply.header.stream_id = stream_id;
        reply.payload = build_response_payload(response.status, response.body);
        if (auto res = session->send(std::move(reply)); !res) {
            handle_session_error(session, res.error());
        }
    }

    ContextConfig config_;
    std::shared_ptr<Dispatcher> dispatcher_;
    std::shared_ptr<Executor> executor_;
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> reactor_running_{false};
    std::thread reactor_thread_;

    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::deque<QueuedFrame> frame_queue_;

    std::mutex sessions_mutex_;
    std::vector<std::shared_ptr<Session>> sessions_;

    std::mutex listeners_mutex_;
    std::vector<std::shared_ptr<ListenerState>> listeners_;

    std::atomic<std::uint64_t> next_session_id_{1};

    void remove_session(const std::shared_ptr<Session>& session)
    {
        std::lock_guard lock(sessions_mutex_);
        auto it = std::remove(sessions_.begin(), sessions_.end(), session);
        sessions_.erase(it, sessions_.end());
    }
};

Context::Context(ContextConfig config)
    : impl_(std::make_unique<Impl>(config))
{
}

Context::~Context() = default;

Result<void> Context::listen(const std::string& path)
{
    return impl_->listen(path);
}

Result<void> Context::connect(const std::string& path)
{
    return impl_->connect(path);
}

Result<void> Context::attach_channel(std::shared_ptr<Channel> channel, bool server_side)
{
    return impl_->attach_channel(std::move(channel), server_side);
}

void Context::set_executor(std::shared_ptr<Executor> exec)
{
    impl_->set_executor(std::move(exec));
}

void Context::start()
{
    impl_->start();
}

void Context::stop()
{
    impl_->stop();
}

void Context::join()
{
    impl_->join();
}

std::size_t Context::run()
{
    return impl_->run();
}

std::size_t Context::run_one()
{
    return impl_->run_one();
}

std::size_t Context::poll()
{
    return impl_->poll();
}

}  // namespace hasten::runtime
