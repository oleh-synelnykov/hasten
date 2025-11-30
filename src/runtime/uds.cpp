#include "hasten/runtime/uds.hpp"

#include "hasten/runtime/error.hpp"

#include <atomic>
#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <mutex>
#include <poll.h>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace hasten::runtime::uds
{
namespace
{

Error make_errno_error(const std::string& prefix, int err)
{
    std::error_code code(err, std::generic_category());
    std::string message = prefix;
    if (!prefix.empty()) {
        message += ": ";
    }
    message += code.message();
    return make_error(std::move(code), std::move(message));
}

inline std::uint64_t hton64(std::uint64_t value)
{
    std::uint32_t high = htonl(static_cast<std::uint32_t>(value >> 32));
    std::uint32_t low = htonl(static_cast<std::uint32_t>(value & 0xffffffffULL));
    return (static_cast<std::uint64_t>(low) << 32) | high;
}

inline std::uint64_t ntoh64(std::uint64_t value)
{
    return hton64(value);
}

class UdsChannel : public Channel
{
public:
    explicit UdsChannel(int fd)
        : fd_(fd)
    {
        wake_fd_ = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (wake_fd_ < 0) {
            throw std::system_error(errno, std::generic_category(), "eventfd");
        }
    }

    ~UdsChannel() override
    {
        close();
        if (wake_fd_ >= 0) {
            ::close(wake_fd_);
        }
    }

    Encoding encoding() const override
    {
        return Encoding::Hb1;
    }

    Result<void> send(Frame frame) override;
    Result<Frame> receive() override;
    void close() override;

private:
    int fd() const
    {
        return fd_.load(std::memory_order_relaxed);
    }

    static Result<void> write_full(int fd, const std::uint8_t* data, std::size_t size)
    {
        std::size_t written = 0;
        while (written < size) {
            ssize_t rc = ::write(fd, data + written, size - written);
            if (rc < 0) {
                int err = errno;
                if (err == EINTR) {
                    continue;
                }
                return unexpected_result(make_errno_error("write", err));
            }
            written += static_cast<std::size_t>(rc);
        }
        return {};
    }

    static Result<void> read_full(int fd, int wake_fd, std::uint8_t* data, std::size_t size)
    {
        std::size_t read_bytes = 0;
        while (read_bytes < size) {
            struct pollfd fds[2];
            fds[0].fd = fd;
            fds[0].events = POLLIN;
            fds[0].revents = 0;
            fds[1].fd = wake_fd;
            fds[1].events = POLLIN;
            fds[1].revents = 0;

            int rc = ::poll(fds, 2, -1);
            if (rc < 0) {

                int err = errno;
                if (err == EINTR) {
                    continue;
                }
                return unexpected_result(make_errno_error("poll", err));
            }

            if (fds[1].revents & POLLIN) {
                std::uint64_t value;
                while (::read(wake_fd, &value, sizeof(value)) > 0) {
                }
                return unexpected_result(ErrorCode::Cancelled, "Channel closed");
            }

            if (fds[0].revents & (POLLERR | POLLNVAL)) {
                int err = errno;
                return unexpected_result(make_errno_error("poll", err));
            }

            if (!(fds[0].revents & (POLLIN | POLLHUP))) {
                continue;
            }

            ssize_t read_rc = ::read(fd, data + read_bytes, size - read_bytes);
            if (read_rc < 0) {
                int err = errno;
                if (err == EINTR) {
                    continue;
                }
                return unexpected_result(make_errno_error("read", err));
            }
            if (read_rc == 0) {
                return unexpected_result(ErrorCode::TransportError, "peer closed connection");
            }
            read_bytes += static_cast<std::size_t>(read_rc);
        }
        return {};
    }

    std::atomic<int> fd_{-1};
    int wake_fd_ = -1;
};

class SimpleDispatcher : public Dispatcher
{
public:
    std::uint64_t open_stream() override
    {
        return next_id_++;
    }

    void close_stream(std::uint64_t stream_id) override
    {
        std::lock_guard lock(mutex_);
        handlers_.erase(stream_id);
    }

    void set_response_handler(std::uint64_t stream_id, rpc::Responder handler) override
    {
        std::lock_guard lock(mutex_);
        handlers_[stream_id] = std::move(handler);
    }

    std::optional<rpc::Responder> take_response_handler(std::uint64_t stream_id) override
    {
        std::lock_guard lock(mutex_);
        auto it = handlers_.find(stream_id);
        if (it == handlers_.end()) {
            return std::nullopt;
        }
        auto handler = std::move(it->second);
        handlers_.erase(it);
        return handler;
    }

private:
    std::atomic<std::uint64_t> next_id_{1};
    std::mutex mutex_;
    std::unordered_map<std::uint64_t, rpc::Responder> handlers_;
};

int create_socket()
{
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::system_error(errno, std::generic_category(), "socket");
    }
    int flags = fcntl(fd, F_GETFD);
    if (flags >= 0) {
        fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
    }
    return fd;
}

sockaddr_un make_address(const std::string& path)
{
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path)) {
        throw std::runtime_error("Unix socket path too long");
    }
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    return addr;
}

Result<std::shared_ptr<Channel>> wrap_fd(int fd)
{
    if (fd < 0) {
        int err = errno;
        return unexpected_result<std::shared_ptr<Channel>>(make_errno_error("accept/connect", err));
    }
    return std::make_shared<UdsChannel>(fd);
}

}  // namespace

Result<void> UdsChannel::send(Frame frame)
{
    int current_fd = fd();
    if (current_fd < 0) {
        return unexpected_result(ErrorCode::TransportError, "Invalid channel file descriptor");
    }

    if (frame.payload.size() > std::numeric_limits<std::uint32_t>::max()) {
        return unexpected_result(ErrorCode::TransportError, "Frame payload too large");
    }
    frame.header.length = static_cast<std::uint32_t>(frame.payload.size());
    std::array<std::uint8_t, FrameHeaderSize> header_buffer{};
    if (auto res = encode_header(frame.header, header_buffer); !res) {
        return res;
    }

    if (auto res = write_full(current_fd, header_buffer.data(), header_buffer.size()); !res) {
        return res;
    }

    if (!frame.payload.empty()) {
        if (auto res = write_full(current_fd, frame.payload.data(), frame.payload.size()); !res) {
            return res;
        }
    }
    return {};
}

Result<Frame> UdsChannel::receive()
{
    int current_fd = fd();
    if (current_fd < 0) {
        return unexpected_result<Frame>(ErrorCode::TransportError, "Invalid channel file descriptor");
    }

    std::array<std::uint8_t, FrameHeaderSize> header_buffer{};
    if (auto res = read_full(current_fd, wake_fd_, header_buffer.data(), header_buffer.size()); !res) {
        return std::unexpected(res.error());
    }

    auto header = decode_header(header_buffer);
    if (!header) {
        return std::unexpected(header.error());
    }

    Frame frame;
    frame.header = *header;
    if (frame.header.length > 0) {
        frame.payload.resize(frame.header.length);
        if (auto res = read_full(current_fd, wake_fd_, frame.payload.data(), frame.payload.size()); !res) {
            return std::unexpected(res.error());
        }
    }
    return frame;
}

void UdsChannel::close()
{
    int old = fd_.exchange(-1, std::memory_order_acq_rel);
    if (old >= 0) {
        ::close(old);
    }
    if (wake_fd_ >= 0) {
        std::uint64_t one = 1;
        ::write(wake_fd_, &one, sizeof(one));
    }
}

Server::Server(int fd, std::string path)
    : fd_(fd)
    , path_(std::move(path))
{
}

Server::~Server()
{
    close();
    if (!path_.empty()) {
        ::unlink(path_.c_str());
    }
}

void Server::close()
{
    int old = fd_;
    fd_ = -1;
    if (old >= 0) {
        ::close(old);
    }
}

Result<std::shared_ptr<Channel>> Server::accept()
{
    if (fd_ < 0) {
        return unexpected_result<std::shared_ptr<Channel>>(ErrorCode::TransportError, "Server socket closed");
    }

    int client_fd = ::accept(fd_, nullptr, nullptr);
    if (client_fd < 0) {
        int err = errno;
        return unexpected_result<std::shared_ptr<Channel>>(make_errno_error("accept", err));
    }
    return wrap_fd(client_fd);
}

Result<std::shared_ptr<Server>> listen(const std::string& path)
{
    try {
        int fd = create_socket();
        sockaddr_un addr = make_address(path);
        ::unlink(path.c_str());
        if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            int err = errno;
            ::close(fd);
            return unexpected_result<std::shared_ptr<Server>>(make_errno_error("bind", err));
        }
        if (::listen(fd, SOMAXCONN) < 0) {
            int err = errno;
            ::close(fd);
            return unexpected_result<std::shared_ptr<Server>>(make_errno_error("listen", err));
        }
        return std::shared_ptr<Server>(new Server(fd, path));
    } catch (const std::exception& ex) {
        return unexpected_result<std::shared_ptr<Server>>(ErrorCode::TransportError, ex.what());
    }
}

Result<std::shared_ptr<Channel>> connect(const std::string& path)
{
    try {
        int fd = create_socket();
        sockaddr_un addr = make_address(path);
        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            int err = errno;
            ::close(fd);
            return unexpected_result<std::shared_ptr<Channel>>(make_errno_error("connect", err));
        }
        return wrap_fd(fd);
    } catch (const std::exception& ex) {
        return unexpected_result<std::shared_ptr<Channel>>(ErrorCode::TransportError, ex.what());
    }
}

std::shared_ptr<Dispatcher> make_dispatcher()
{
    return std::make_shared<SimpleDispatcher>();
}

Result<std::pair<std::shared_ptr<Channel>, std::shared_ptr<Channel>>> socket_pair()
{
    int fds[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
        return unexpected_result<std::pair<std::shared_ptr<Channel>, std::shared_ptr<Channel>>>(
            make_errno_error("socketpair", errno));
    }

    auto first = wrap_fd(fds[0]);
    if (!first) {
        ::close(fds[1]);
        return std::unexpected(first.error());
    }

    auto second = wrap_fd(fds[1]);
    if (!second) {
        return std::unexpected(second.error());
    }

    return std::make_pair(*first, *second);
}

}  // namespace hasten::runtime::uds
