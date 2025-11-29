#pragma once

#include "hasten/runtime/channel.hpp"
#include "hasten/runtime/result.hpp"

#include <memory>
#include <string>

namespace hasten::runtime::uds
{

class Server
{
public:
    ~Server();
    Result<std::shared_ptr<Channel>> accept();
    void close();

private:
    friend Result<std::shared_ptr<Server>> listen(const std::string& path);
    explicit Server(int fd, std::string path);
    int fd_ = -1;
    std::string path_;
};

Result<std::shared_ptr<Server>> listen(const std::string& path);
Result<std::shared_ptr<Channel>> connect(const std::string& path);
std::shared_ptr<Dispatcher> make_dispatcher();
Result<std::pair<std::shared_ptr<Channel>, std::shared_ptr<Channel>>> socket_pair();

}  // namespace hasten::runtime::uds
