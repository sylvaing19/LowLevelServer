#include "SocketInterface.h"
#include <cerrno>
#include <cstring>

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
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
    int ret;

    // If socket already created, return error
    if (m_fd >= 0) {
        return -EEXIST;
    }

    // Create the socket
    m_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_fd < 0) {
        return -errno;
    }

    // Set socket as NON-BLOCKING
#ifdef _WIN32
    u_long mode = 1;
    ret = ioctlsocket(m_fd, FIONBIO, &mode); // NOLINT(hicpp-signed-bitwise)
    if (ret != 0) {
        return -1;
    }
#else
    ret = fcntl(m_fd, F_SETFL, O_NONBLOCK);
    if (ret < 0) {
        return -errno;
    }
#endif

    // Address and port conversion (string to binary data)
    sockaddr_in server_address = {};
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);

    ret = inet_pton(AF_INET, address_string, &server_address.sin_addr);
    if (ret < 0) {
        return -errno;
    } else if (ret != 1) {
        return -EINVAL;
    }

    // Set option: reusable addresses and ports
#ifdef _WIN32
    int option_name = SO_REUSEADDR;
#else
    int option_name = (SO_REUSEADDR | SO_REUSEPORT);
#endif
    int option_value = 1;
    ret = setsockopt(m_fd, SOL_SOCKET, option_name,
            (const char *)(&option_value), sizeof(option_value));
    if (ret < 0) {
        return -errno;
    }

    // Bind socket to address
    ret = bind(m_fd, (sockaddr*)(&server_address), sizeof(server_address));
    if (ret < 0) {
        return -errno;
    }

    // Start listening
    ret = listen(m_fd, SOCK_INTERFACE_MAX_CLIENTS);
    if (ret < 0) {
        return -errno;
    }

    return 0;
}

int SocketInterface::close()
{
    return 0;
}

int SocketInterface::receive()
{
    return 0;
}

int SocketInterface::available() const
{
    return m_msg_queue.size();
}

LowLevelMessage SocketInterface::getLastMessage()
{
    LowLevelMessage ret = m_msg_queue.front();
    m_msg_queue.pop();
    return ret;
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
