#include "hasten/runtime/rpc.hpp"

#include <mutex>
#include <unordered_map>

namespace hasten::runtime::rpc
{
namespace
{

class Registry
{
public:
    static Registry& instance()
    {
        static Registry reg;
        return reg;
    }

    void register_handler(std::uint64_t interface_id, Handler handler)
    {
        std::lock_guard lock(mutex_);
        handlers_[interface_id] = std::move(handler);
    }

    std::optional<Handler> find_handler(std::uint64_t interface_id)
    {
        std::lock_guard lock(mutex_);
        auto it = handlers_.find(interface_id);
        if (it == handlers_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

private:
    std::mutex mutex_;
    std::unordered_map<std::uint64_t, Handler> handlers_;
};

}  // namespace

void register_handler(std::uint64_t interface_id, Handler handler)
{
    Registry::instance().register_handler(interface_id, std::move(handler));
}

std::optional<Handler> find_handler(std::uint64_t interface_id)
{
    return Registry::instance().find_handler(interface_id);
}

}  // namespace hasten::runtime::rpc
