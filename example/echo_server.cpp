#include "server/rpc_server.h"
#include "example/echo_service.h"

int main()
{
    dRPC::RpcServerOptions options(8888);
    dRPC::RpcServer rpc_server(options);

    // 注册服务
    EchoServiceImpl echo_service;
    rpc_server.register_service("EchoService", &echo_service);

    rpc_server.start();
    return 0;
}
