#pragma once
#include "minirpc/net/Poller.h"

namespace minirpc::net{


class Channel;
class EventLoop
{


public:
    EventLoop();
    ~EventLoop()=default;
    void UpdateChannel(Channel*ch);
    void RemoveChannel(Channel*ch);
    void Loop();
    void Stop()noexcept;
private:
    Poller ep_;
    bool stop_;
    Poller::ChannelList channels;
    
};


}