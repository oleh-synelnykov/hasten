#include "hasten/runtime/uds.hpp"

#include "hasten/runtime/error.hpp"

#include <atomic>
#include <cerrno>
#include <cstring>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
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
    }

    ~UdsChannel() override
    {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    Encoding encoding() const override
    {
        return Encoding::Hb1;
    }

    Result<void> send(Frame frame) override
    {
        if (fd_ < 0) {
            return unexpected_result(ErrorCode::TransportError, "Invalid channel file descriptor");
        }

        std::uint32_t payload_size = static_cast<std::uint32_t>(frame.payload.size());
        std::uint32_t type = htonl(frame.type);
        std::uint32_t flags = htonl(frame.flags);
        std::uint64_t stream = hton64(frame.stream_id);

        std::vector<std::uint8_t> buffer;
        buffer.reserve(sizeof(type) + sizeof(flags) + sizeof(stream) + sizeof(payload_size) +
                       frame.payload.size());

        auto append = [&buffer](const auto& value) {
            const std::uint8_t* ptr = reinterpret_cast<const std::uint8_t*>(&value);
            buffer.insert(buffer.end(), ptr, ptr + sizeof(value));
        };

        append(type);
        append(flags);
        append(stream);
        append(payload_size);
        buffer.insert(buffer.end(), frame.payload.begin(), frame.payload.end());

        ssize_t written = ::write(fd_, buffer.data(), buffer.size());
        if (written < 0 || static_cast<std::size_t>(written) != buffer.size()) {
            int err = errno;
            return unexpected_result(make_errno_error("write", err));
        }
        return {};
    }

private:
    int fd_ = -1;
};

class SimpleDispatcher : public Dispatcher
{
public:
    std::uint64_t open_stream() override
    {
        return next_id_++;
    }

    void close_stream(std::uint64_t) override {}

private:
    std::atomic<std::uint64_t> next_id_{1};
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

Server::Server(int fd, std::string path)
    : fd_(fd)
    , path_(std::move(path))
{
}

Server::~Server()
{
    if (fd_ >= 0) {
        ::close(fd_);
    }
    if (!path_.empty()) {
        ::unlink(path_.c_str());
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

}  // namespace hasten::runtime::uds
