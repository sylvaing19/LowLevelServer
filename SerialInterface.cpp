#include "SerialInterface.h"

SerialInterface::SerialInterface()
{
    m_fd = -1;
}

SerialInterface::~SerialInterface() = default;

int SerialInterface::open(const char *port)
{
    return 0;
}

int SerialInterface::close()
{
    return 0;
}

int SerialInterface::receive()
{
    return 0;
}

int SerialInterface::available() const
{
    return m_msg_queue.size();
}

LowLevelMessage SerialInterface::getLastMessage()
{
    LowLevelMessage ret = m_msg_queue.front();
    m_msg_queue.pop();
    return ret;
}

int SerialInterface::sendMessage(const LowLevelMessage &message)
{
    return 0;
}
