#pragma once
#include "minirpc/net/Poller.h"

#include <functional>
#include <vector>

namespace minirpc::net{


class Channel;
class EventLoop
{


public:
    using Functor=std::function<void()>;

    EventLoop();
    ~EventLoop()=default;
    void UpdateChannel(Channel*ch);
    void RemoveChannel(Channel*ch);
    void QueueInLoop(Functor cb);
    void Loop();
    void Stop()noexcept;
private:
    void DoPendingFunctors();

    Poller ep_;
    bool stop_;
    Poller::ChannelList channels;
    std::vector<Functor> pending_functors_;
    
};


}
