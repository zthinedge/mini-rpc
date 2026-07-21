#pragma once
#include "minirpc/net/Channel.h"
#include "minirpc/net/Poller.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace minirpc::net{


class EventLoop
{


public:
    using Functor=std::function<void()>;

    EventLoop();
    ~EventLoop();
    void UpdateChannel(Channel*ch);
    void RemoveChannel(Channel*ch);
    void RunInLoop(Functor cb);
    void QueueInLoop(Functor cb);
    void Loop();
    void Stop()noexcept;
    bool IsInLoopThread()const noexcept;
private:
    void DoPendingFunctors();
    void Wakeup()noexcept;
    void HandleWakeup()noexcept;

    Poller ep_;
    std::atomic_bool stop_;
    Poller::ChannelList channels;

    const std::thread::id thread_id_;
    int wakeup_fd_;
    Channel wakeup_channel_;

    std::mutex mutex_;
    std::vector<Functor> pending_functors_;
    bool calling_pending_functors_;
    
};


}
