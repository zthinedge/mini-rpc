#pragma once

#include "calculator.pb.h"
#include "minirpc/rpc/CallOptions.h"

namespace minirpc::rpc{
class RpcClient;
}

namespace minirpc::example::calculator{

class CalculatorStub{
public:
    explicit CalculatorStub(rpc::RpcClient* client);

    AddResponse Add(
        const AddRequest& request,
        rpc::CallOptions options={}
    )const;

private:
    rpc::RpcClient* client_;
};

}
