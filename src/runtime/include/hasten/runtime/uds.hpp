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

private:
    friend Result<std::shared_ptr<Server>> listen(const std::string& path);
    explicit Server(int fd, std::string path);
    int fd_ = -1;
    std::string path_;
};

Result<std::shared_ptr<Server>> listen(const std::string& path);
Result<std::shared_ptr<Channel>> connect(const std::string& path);
std::shared_ptr<Dispatcher> make_dispatcher();

}  // namespace hasten::runtime::uds
