#include "server/rpc_server.h"

int main() {
    dRPC::RpcServerOptions options(8888);
    dRPC::RpcServer rpc_server(options);

    rpc_server.start();
    return 0;
}
