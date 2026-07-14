#include "minirpc/net/EventLoop.h"
#include "minirpc/net/Channel.h"

#include <utility>

namespace minirpc::net{
EventLoop::EventLoop():ep_(this),stop_(false){}

void EventLoop::UpdateChannel(Channel*ch){
    ep_.UpdateChannel(ch);
}
void EventLoop::RemoveChannel(Channel*ch){
    ep_.RemoveChannel(ch);
}

void EventLoop::QueueInLoop(Functor cb){
    pending_functors_.push_back(std::move(cb));
}

void EventLoop::Loop(){
    while(!stop_){
        ep_.Poll(-1,&channels);

        for(auto ch:channels){
            ch->HandleEvent();
        }

        channels.clear();
        DoPendingFunctors();
    }
}

void EventLoop::DoPendingFunctors(){
    std::vector<Functor> functors;
    functors.swap(pending_functors_);

    for(auto& functor:functors){
        functor();
    }
}

void EventLoop::Stop()noexcept{
    stop_=true;
}
}
