#pragma once

#include <cstdint>

#include "LowLevelMessage.h"
#include "SocketInterface.h"
#include "SerialInterface.h"

class MessageRouter
{
public:
    MessageRouter();
    ~MessageRouter();

    void setSocketAddress(const char * ip_address);
    void setSocketPort(uint16_t port);
    void setSerialPort(const char * serial_port);

    int open();
    int close();

    /* Returns 0 on normal operation, -1 in case of error.
     * In case of error, the object is always closed, so open()
     * must be called to re-enable communication */
    int communicate();

private:
    void processMsgFromSerial(const LowLevelMessage &msg);
    int processMsgFromSocket(const LowLevelMessage &msg);

    bool m_opened;
    const char *m_ip_address;
    uint16_t m_tcp_port;
    const char *m_serial_port;

    SocketInterface m_socket_interface;
    SerialInterface m_serial_interface;

    uint32_t m_subscriptions[SOCK_INTERFACE_MAX_CLIENTS];
};
