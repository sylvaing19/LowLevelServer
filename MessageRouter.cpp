#include "MessageRouter.h"

#include <cerrno>

#define DEFAULT_SUBSCRIPTION 0x06

MessageRouter::MessageRouter()
{
    m_opened = false;
    m_ip_address = nullptr;
    m_tcp_port = 0;
    m_serial_port = nullptr;
    for (uint32_t &subscription : m_subscriptions) {
        subscription = DEFAULT_SUBSCRIPTION;
    }
}

MessageRouter::~MessageRouter() = default;

void MessageRouter::setSocketAddress(const char *ip_address)
{
    m_ip_address = ip_address;
}

void MessageRouter::setSocketPort(uint16_t port)
{
    m_tcp_port = port;
}

void MessageRouter::setSerialPort(const char *serial_port)
{
    m_serial_port = serial_port;
}

int MessageRouter::open()
{
    if (m_opened) {
        return 0;
    }

    if (m_ip_address == nullptr || m_serial_port == nullptr) {
        return -EFAULT;
    }

    int ret;

    ret = m_serial_interface.open(m_serial_port);
    if (ret < 0) {
        return ret;
    }

    ret = m_socket_interface.open(m_ip_address, m_tcp_port);
    if (ret < 0) {
        return ret;
    }

    m_opened = true;
    return 0;
}

int MessageRouter::close()
{
    if (!m_opened) {
        return 0;
    }

    int ret_a = m_socket_interface.close();
    int ret_b = m_serial_interface.close();

    for (uint32_t &subscription : m_subscriptions) {
        subscription = DEFAULT_SUBSCRIPTION;
    }
    m_opened = false;

    if (ret_a < 0 || ret_b < 0) {
        return -1;
    } else {
        return 0;
    }
}

int MessageRouter::communicate()
{
    if (!m_opened) {
        return -ENOTCONN;
    }

    int ret;

    /* Receive on serial port */
    ret = m_serial_interface.receive();
    if (ret < 0) {
        close();
        return ret;
    }
    while (m_serial_interface.available() > 0) {
        processMsgFromSerial(m_serial_interface.getLastMessage());
    }

    /* Receive on socket */
    m_socket_interface.receive();
    while (m_socket_interface.available() > 0) {
        ret = processMsgFromSocket(m_socket_interface.getLastMessage());
        if (ret < 0) {
            close();
            return ret;
        }
    }

    return 0;
}

void MessageRouter::processMsgFromSerial(const LowLevelMessage &msg)
{
}

int MessageRouter::processMsgFromSocket(const LowLevelMessage &msg)
{
    return 0;
}
