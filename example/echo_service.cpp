#include "example/echo_service.h"
#include "util/common.h"

void EchoServiceImpl::Echo(
    google::protobuf::RpcController *controller,
    const ::EchoRequest *request,
    ::EchoResponse *response,
    ::google::protobuf::Closure *done)
{
    std::string message = "[Echo] " + request->message();
    info("Echo: {}", message);
    response->set_message(message);
}

void EchoServiceImpl::Echo1(
    google::protobuf::RpcController *controller,
    const ::EchoRequest *request,
    ::EchoResponse *response,
    ::google::protobuf::Closure *done)
{
    std::string message = "[Echo1] " + request->message();
    info("Echo1: {}", message);
    response->set_message(message);
}