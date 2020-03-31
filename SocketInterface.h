#pragma once

#include <cstdint>
#include <queue>
#include "LowLevelMessage.h"

#define SOCK_INTERFACE_MAX_CLIENTS 32
#define SOCK_INTERFACE_BUFFER_SIZE 1024

class SocketInterface
{
public:
    SocketInterface();
    ~SocketInterface();

    int open(uint16_t server_port);
    int close();
    void receive();
    int available() const;
    LowLevelMessage getLastMessage();
    void sendMessage(const LowLevelMessage &message, int cid = UNKNOWN_CLIENT_ID);

private:
    int registerClient(int fd);
    int freeClient(size_t id);

    struct Client {
        Client();
        int fd;
        LowLevelMessage message;
    };

    int m_fd;
    Client m_clients[SOCK_INTERFACE_MAX_CLIENTS];
    std::queue<LowLevelMessage> m_msg_queue;
    uint8_t m_buffer[SOCK_INTERFACE_BUFFER_SIZE];
};
