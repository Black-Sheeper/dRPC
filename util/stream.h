#pragma once

#include <google/protobuf/io/zero_copy_stream.h>

#include "chained_buffer.h"

namespace dRPC::util
{
    class InputStream : public google::protobuf::io::ZeroCopyInputStream
    {
    public:
        InputStream(ChainedBuffer<> *input_buffer) : input_buffer_(input_buffer) {}
        ~InputStream() = default;

        size_t read(void *buf, size_t len)
        {
            return input_buffer_->read(buf, len);
        }

        void push_limit(int limit)
        {
            input_buffer_->push_limit(limit);
        }

        void pop_limit()
        {
            input_buffer_->pop_limit();
        }

        bool Next(const void **data, int *size) override
        {
            return input_buffer_->input_next(data, size);
        }

        void BackUp(int count) override
        {
            input_buffer_->input_back_up(count);
        }

        bool Skip(int count) override
        {
            return input_buffer_->input_skip(count);
        }

        int64_t ByteCount() const override
        {
            return input_buffer_->input_byte_count();
        }

    private:
        ChainedBuffer<> *input_buffer_;

        InputStream(const InputStream &) = delete;
        InputStream &operator=(const InputStream &) = delete;
    };

    class OutputStream : public google::protobuf::io::ZeroCopyOutputStream
    {
    public:
        OutputStream(ChainedBuffer<> *output_buffer) : output_buffer_(output_buffer) {}
        ~OutputStream() = default;

        size_t write(const void *data, size_t len)
        {
            return output_buffer_->write(data, len);
        }

        bool Next(void **data, int *size) override
        {
            return output_buffer_->output_next(data, size);
        }

        void BackUp(int count) override
        {
            output_buffer_->output_back_up(count);
        }

        int64_t ByteCount() const override
        {
            return output_buffer_->output_byte_count();
        }

    private:
        ChainedBuffer<> *output_buffer_;

        OutputStream(const OutputStream &) = delete;
        OutputStream &operator=(const OutputStream &) = delete;
    };
}