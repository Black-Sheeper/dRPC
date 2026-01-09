#include <string_view>
#include <thread>

#include "util/common.h"
#include "util/stream.h"
#include "scheduler/scheduler.h"
#include "client/client_channel.h"
#include "util/service.h"
#include "example/echo.pb.h"

using namespace dRPC;

void handle_response(EchoResponse *response)
{
    info("response: {}", response->DebugString());
    delete response;
}

int main()
{
    Scheduler scheduler(1000);
    auto executor = scheduler.alloc_executor();
    ClientOptions client_options;
    client_options.ip_ = "127.0.0.1";
    client_options.port_ = 8888;
    ClientChannel channel(client_options, executor);

    std::string data("echo request");
    for (int i = 0; i <= 1000; ++i)
    {
        EchoService_Stub stub(&channel);
        auto request = new EchoRequest;
        request->set_message(data.data() + std::to_string(i));
        auto response = new EchoResponse;
        auto controller = new RpcController;
        auto done = google::protobuf::NewCallback(handle_response, response);
        stub.Echo1(controller,request,response,done);
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));
    scheduler.stop();
}
