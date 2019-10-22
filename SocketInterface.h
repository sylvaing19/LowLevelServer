#pragma once

#include <cstdint>
#include <queue>
#include "LowLevelMessage.h"

#define SOCK_INTERFACE_MAX_CLIENTS 32

class SocketInterface
{
public:
    SocketInterface();
    ~SocketInterface();

    int open(const char *address_string, uint16_t server_port);
    int close();
    int listen();
    int available() const;
    LowLevelMessage getLastMessage();
    int sendMessage(const LowLevelMessage &message);

private:
    struct Client {
        Client();
        int fd;
        LowLevelMessage message;
    };

    int m_fd;
    Client m_clients[SOCK_INTERFACE_MAX_CLIENTS];
    std::queue<LowLevelMessage> m_msg_queue;
};
