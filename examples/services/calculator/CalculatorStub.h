#pragma once

#include "calculator.pb.h"

namespace minirpc::rpc{
class RpcClient;
}

namespace minirpc::example::calculator{

class CalculatorStub{
public:
    explicit CalculatorStub(rpc::RpcClient* client);

    AddResponse Add(const AddRequest& request)const;

private:
    rpc::RpcClient* client_;
};

}
