#include "server/rpc_server.h"

int main() {
    dRPC::server::RpcServerOptions options(8888);
    dRPC::server::RpcServer rpc_server(options);

    rpc_server.start();
    return 0;
}
