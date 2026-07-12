#include "minirpc/net/EventLoop.h"
#include "minirpc/net/Channel.h"
namespace minirpc::net{
EventLoop::EventLoop():ep_(this),stop_(false){}

void EventLoop::UpdateChannel(Channel*ch){
    ep_.UpdateChannel(ch);
}
void EventLoop::RemoveChannel(Channel*ch){
    ep_.RemoveChannel(ch);
}

void EventLoop::Loop(){
    while(!stop_){
        ep_.Poll(-1,&channels);

        for(auto ch:channels){
            ch->HandleEvent();
        }
    }
}

void EventLoop::Stop()noexcept{
    stop_=true;
}
}