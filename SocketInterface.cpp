#include "SocketInterface.h"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#endif

SocketInterface::SocketInterface()
{
    m_fd = -1;
    for (int i = 0; i < SOCK_INTERFACE_MAX_CLIENTS; i++) {
        m_clients[i].message.set_client_id(i);
    }
}

SocketInterface::~SocketInterface() = default;

int SocketInterface::open(const char *address_string, uint16_t server_port)
{
    return 0;
}

int SocketInterface::close()
{
    return 0;
}

int SocketInterface::listen()
{
    return 0;
}

int SocketInterface::available() const
{
    return m_msg_queue.size();
}

LowLevelMessage SocketInterface::getLastMessage()
{
    return LowLevelMessage(LL_MSG_SIDE_SOCKET);
}

int SocketInterface::sendMessage(const LowLevelMessage &message)
{
    return 0;
}

SocketInterface::Client::Client() :
        message(LL_MSG_SIDE_SOCKET)
{
    fd = -1;
}
