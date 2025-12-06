#include "connection.h"

#include <sys/uio.h>

namespace dRPC::net
{
    dRPC::ReadAwaiter Connection::async_read()
    {
        auto [buffer, maxsize] = read_buf_.write_view();
        int read = 0;
        while (maxsize > 0)
        {
            int n = ::read(fd(), buffer, maxsize);
            if (n > 0)
            {
                read += n;
                buffer += n;
                maxsize -= n;
                continue;
            }
            else if (n == 0)
            {
                close();
                break;
            }
            else
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    break;
                }
                if (errno == EINTR)
                {
                    continue;
                }
                error("read data failed, errno: {}", errno);
                close();
                break;
            }
        }
        if (closed())
        {
            resume_write();
        }
        read_buf_.commit_resv(read);
        bool should_suspend = !closed() && read <= 0;
        return {this, should_suspend};
    }

    dRPC::WriteAwaiter Connection::async_write()
    {
        int need_write = to_write_bytes();
        int written = 0;
        while (written < need_write)
        {
            auto iovs = write_buf_.get_iovecs();
            int n = ::writev(fd(), iovs.data(), iovs.size());
            if (n >= 0)
            {
                written += n;
                continue;
            }
            else
            {
                if (errno == EINTR)
                {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    break;
                }
                error("write data failed, errno: {}", errno);
                break;
            }
        }
        write_buf_.commit_send(written);
        bool should_suspend = !closed() && written < need_write;
        return {this, should_suspend};
    }
}