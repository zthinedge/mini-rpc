#include "calculator.pb.h"

#include <cassert>
#include <string>

using minirpc::example::calculator::AddRequest;
using minirpc::example::calculator::AddResponse;

namespace{

void TestRequestRoundTrip(){
    AddRequest input;
    input.set_a(10);
    input.set_b(20);

    std::string payload;
    assert(input.SerializeToString(&payload));

    AddRequest output;
    assert(output.ParseFromString(payload));
    assert(output.a()==10);
    assert(output.b()==20);
}

void TestResponseRoundTrip(){
    AddResponse input;
    input.set_result(30);

    std::string payload;
    assert(input.SerializeToString(&payload));

    AddResponse output;
    assert(output.ParseFromString(payload));
    assert(output.result()==30);
}

}

int main(){
    TestRequestRoundTrip();
    TestResponseRoundTrip();
    return 0;
}
