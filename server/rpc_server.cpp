#include "rpc_server.h"

#include "util/common.h"
#include "proto/message.pb.h"

namespace dRPC
{
    RpcServer::RpcServer(const RpcServerOptions &options)
        : options_(options), accepter_(options.port_, options.backlog_, options.nodelay_)
    {
        scheduler_ = std::make_unique<dRPC::Scheduler>(options.timeout_);
    }

    void RpcServer::start()
    {
        while (true)
        {
            int connfd = accepter_.accept();
            if (connfd == -1)
            {
                if (errno != EINTR)
                {
                    error("accept failed: {}", strerror(errno));
                }
                continue;
            }

            auto executor = scheduler_->alloc_executor();
            auto conn = std::make_shared<dRPC::net::Connection>(connfd, executor);

            executor->spawn([this, conn]()
                            { send_fn(conn); });
            executor->spawn([this, conn]()
                            { recv_fn(conn); });
        }
    }

    dRPC::Task RpcServer::recv_fn(std::shared_ptr<net::Connection> conn)
    {
        co_await dRPC::RegisterReadAwaiter{conn.get()};

        while (true)
        {
            auto input_stream = conn->get_input_stream();

            while (conn->to_read_bytes() < sizeof(uint32_t) && !conn->closed())
            {
                co_await conn->async_read();
            }
            if (conn->closed() && conn->to_read_bytes() < sizeof(uint32_t))
            {
                break;
            }
            uint32_t header_len;
            if (!input_stream.read(&header_len, sizeof(uint32_t)))
            {
                error("Failed to read header len");
                break;
            }

            while (conn->to_read_bytes() < header_len && !conn->closed())
            {
                co_await conn->async_read();
            }
            if (conn->closed() && conn->to_read_bytes() < header_len)
            {
                break;
            }
            proto::Header header;
            input_stream.push_limit(header_len);
            if(!header.ParseFromZeroCopyStream(&input_stream)){
                error("Failed to parse header");
                break;
            }
            input_stream.pop_limit();

            if(header.magic()!=MAGIC_NUM){
                error("Invalid magic number: 0x{:08x}",header.magic());
                break;
            }
            if(header.version()!=VERSION){
                error("Invalid version: {}",header.version());
                break;
            }

            while(conn->to_read_bytes()<sizeof(uint32_t)&&!conn->closed()){
                co_await conn->async_read();
            }
            if(conn->closed()&&conn->to_read_bytes()<sizeof(uint32_t)){
                break;
            }
            uint32_t request_len;
            if(!input_stream.read(&request_len,sizeof(uint32_t))){
                error("Failed to read request len");
                break;
            }

            const auto& service_name=header.service_name();
            const auto& method_name=header.method_name();

            auto iter=service_registry_.find(service_name);
            if(iter==service_registry_.end()){
                error("Service not found: {}",service_name);
                break;
            }
            auto service=iter->second;
            auto method=service->GetDescriptor()->FindMethodByName(method_name);
            if(method==nullptr){
                error("Method not found: {}",method_name);
                break;
            }

            std::unique_ptr<google::protobuf::Message> request(service->GetRequestPrototype(method).New());
            std::unique_ptr<google::protobuf::Message> response(service->GetResponsePrototype(method).New());

            while(conn->to_read_bytes()<request_len){
                co_await conn->async_read();
            }
            if(conn->closed()&&conn->to_read_bytes()<request_len){
                break;
            }
            input_stream.push_limit(request_len);
            if(!request->ParseFromZeroCopyStream(&input_stream)){
                error("Failed to parse request");
                break;
            }
            input_stream.pop_limit();

            service->CallMethod(method,nullptr,request.get(),response.get(),nullptr);

            auto output_stream=conn->get_output_stream();

            proto::Header resp_header;
            resp_header.set_magic(MAGIC_NUM);
            resp_header.set_version(VERSION);
            resp_header.set_message_type(proto::MessageType::RESPONSE);
            resp_header.set_request_id(header.request_id());
            uint32_t resp_header_len=resp_header.ByteSizeLong();
            output_stream.write(&resp_header_len,sizeof(resp_header_len));
            resp_header.SerializeToZeroCopyStream(&output_stream);

            uint32_t response_len=response->ByteSizeLong();
            output_stream.write(&response_len,sizeof(response_len));
            response->SerializeToZeroCopyStream(&output_stream);

            conn->resume_write();
        }

        if(!conn->closed()){
            conn->close();
        }
        info("connection[{}] closed by peer: {}:{}",conn->fd(),conn->socket()->peer_addr(),conn->socket()->peer_port());
        info("connection[{}] recv_fn done",conn->fd());
    }

    dRPC::Task RpcServer::send_fn(std::shared_ptr<net::Connection> conn)
    {
        while (!conn->closed())
        {
            co_await dRPC::WaitWriteAwaiter{conn.get()};
            co_await conn->async_write();
        }
        info("connection[{}] send_fn done", conn->fd());
    }
}