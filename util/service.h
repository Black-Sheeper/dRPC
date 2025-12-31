#pragma once

#include <string>
#include <google/protobuf/service.h>

namespace dRPC
{
    class RpcController : public google::protobuf::RpcController
    {
    public:
        RpcController() = default;
        ~RpcController() override = default;

        void Reset() override;
        bool Failed() const override { return failed_; }
        std::string ErrorText() const override { return error_text_; }
        void StartCancel() override;
        void SetFailed(const std::string &reason) override;
        bool IsCanceled() const override { return canceled_; }
        void NotifyOnCancel(google::protobuf::Closure *callback) override;

        void SetTimeout(int64_t ms);
        int64_t timeout_ms() const { return timeout_ms_; }

    private:
        bool failed_ = false;
        bool canceled_ = false;
        std::string error_text_;
        int64_t timeout_ms_ = -1; // -1表示无超时
    };
}