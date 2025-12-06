#pragma once

namespace dRPC::net
{
    class Accepter
    {
    public:
        Accepter(int port, int backlog, int nodelay);

        ~Accepter();

        int accept();

    private:
        int sockfd_;
        int port_;
        int backlog_;
        int nodelay_;

        void set_sock_param(int client_fd);

        Accepter(const Accepter &) = delete;
        Accepter &operator=(const Accepter &) = delete;
    };
}