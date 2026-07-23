#include "CalculatorService.h"
#include "minirpc/log/AsyncLogger.h"
#include "minirpc/log/LogMacros.h"
#include "minirpc/net/EventLoop.h"
#include "minirpc/net/InetAddress.h"
#include "minirpc/rpc/RpcServer.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

using namespace minirpc;
using namespace minirpc::example::calculator;

namespace{

class CalculatorServiceImpl:public CalculatorService{
public:
    explicit CalculatorServiceImpl(log::AsyncLogger* logger)
        :logger_(logger){}

    void Add(
        const AddRequest& request,
        AddResponse* response
    )override{
        int result=request.a()+request.b();
        response->set_result(result);

        MINIRPC_LOG_INFO(
            *logger_,
            "CalculatorService.Add: "+
            std::to_string(request.a())+'+'+
            std::to_string(request.b())+'='+
            std::to_string(result)
        );
    }

private:
    log::AsyncLogger* logger_;
};

std::uint16_t ParsePort(int argc,char* argv[]){
    if(argc<2){
        return 9000;
    }

    int port=std::stoi(argv[1]);
    if(port<1||port>65535){
        throw std::invalid_argument("port must be between 1 and 65535");
    }
    return static_cast<std::uint16_t>(port);
}

}

int main(int argc,char* argv[]){
    try{
        std::uint16_t port=ParsePort(argc,argv);

        log::LoggerOptions log_options;
        log_options.file_path="logs/calculator_server.log";
        log_options.roll_size_bytes=1024*1024;
        log::AsyncLogger logger(log_options);

        net::EventLoop loop;
        net::InetAddress address("0.0.0.0",port);
        rpc::RpcServer server(&loop,address);

        CalculatorServiceImpl service(&logger);
        CalculatorServiceAdapter adapter(&service);
        adapter.RegisterTo(&server);
        server.Start();

        MINIRPC_LOG_INFO(
            logger,
            "calculator rpc server listening on port "+
            std::to_string(port)
        );

        std::cout<<"calculator server listening on 0.0.0.0:"
                 <<port<<'\n'
                 <<"press Enter to stop the server\n";

        std::thread stop_thread([&loop](){
            std::string line;
            std::getline(std::cin,line);
            loop.Stop();
        });

        loop.Loop();
        stop_thread.join();

        MINIRPC_LOG_INFO(logger,"calculator rpc server stopped");
        logger.Stop();
        return 0;
    }catch(const std::exception& error){
        std::cerr<<"server error: "<<error.what()<<'\n';
        return 1;
    }
}
