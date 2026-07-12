#pragma once
namespace minirpc::net{

class Channel;
class EventLoop
{
private:
    
public:
    EventLoop(/* args */);
    ~EventLoop();
    void UpdateChannel(Channel*ch);
    void RemoveChannel(Channel*ch);
};


}