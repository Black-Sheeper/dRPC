#pragma once

#include "example/echo.pb.h"

class EchoServiceImpl : public ::EchoService
{
public:
    ~EchoServiceImpl() override = default;

    void Echo(google::protobuf::RpcController *controller,
              const ::EchoRequst *request,
              ::EchoResponse *response,
              ::google::protobuf::Closure *done) override;

    void Echo1(google::protobuf::RpcController *controller,
               const ::EchoRequst *request,
               ::EchoResponse *response,
               ::google::protobuf::Closure *done) override;
};