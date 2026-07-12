#pragma once
namespace minirpc::net{

class Channel;
class EventLoop
{
private:
    /* data */
public:
    EventLoop(/* args */);
    ~EventLoop();
    void UpdateChannel(Channel*ch);
};

EventLoop::EventLoop(/* args */)
{
}

EventLoop::~EventLoop()
{
}
}