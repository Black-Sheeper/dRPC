#include <string_view>
#include <thread>

#include "util/common.h"
#include "util/stream.h"
#include "scheduler/scheduler.h"
#include "client/client_channel.h"

int main() {
    dRPC::Scheduler scheduler(1000);
    auto executor = scheduler.alloc_executor();
    dRPC::ClientOptions client_options;
    client_options.ip_ = "127.0.0.1";
    client_options.port_ = 8888;
    dRPC::ClientChannel channel(client_options, executor);

    std::this_thread::sleep_for(std::chrono::seconds(5));
    scheduler.stop();
}
