#pragma once

#include <cstdint>
#include <queue>
#include "LowLevelMessage.h"

#define SERIAL_INTERFACE_BUFFER_SIZE 1024

class SerialInterface
{
public:
    SerialInterface();
    ~SerialInterface();

    int open(const char *port);
    int close();
    int receive();
    int available() const;
    LowLevelMessage getLastMessage();
    int sendMessage(const LowLevelMessage &message);

private:
    int m_fd;
    LowLevelMessage m_ll_msg;
    std::queue<LowLevelMessage> m_msg_queue;
    uint8_t m_buffer[SERIAL_INTERFACE_BUFFER_SIZE];
};
