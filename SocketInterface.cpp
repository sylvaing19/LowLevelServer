#include "SocketInterface.h"

#include <cstdio>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

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
        printf("Socket interface already opened\n");
        return -EEXIST;
    }

    // Create the socket (non blocking)
    m_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
    if (m_fd < 0) {
        printf("Failed to create socket: %d (%s)\n", -errno, strerror(errno));
        return -errno;
    }

    // Address and port conversion (string to binary data)
    sockaddr_in server_address = {};
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);

    ret = inet_pton(AF_INET, address_string, &server_address.sin_addr);
    if (ret < 0) {
        printf("Failed to convert IP address string: %d (%s)\n", -errno,
                strerror(errno));
        ret = -errno;
        close();
        return ret;
    } else if (ret != 1) {
        printf("Invalid IP address string provided\n");
        close();
        return -EINVAL;
    }

    // Set option: reusable addresses and ports
    int option_value = 1;
    ret = setsockopt(m_fd, SOL_SOCKET, (SO_REUSEADDR | SO_REUSEPORT),
            (const char *)(&option_value), sizeof(option_value));
    if (ret < 0) {
        printf("Failed to set socket options: %d (%s)\n", -errno,
                strerror(errno));
        ret = -errno;
        close();
        return ret;
    }

    // Bind socket to address
    ret = bind(m_fd, (sockaddr*)(&server_address), sizeof(server_address));
    if (ret < 0) {
        printf("Failed to perform socket binding: %d (%s)\n", -errno,
                strerror(errno));
        ret = -errno;
        close();
        return ret;
    }

    // Start listening
    ret = listen(m_fd, SOCK_INTERFACE_MAX_CLIENTS);
    if (ret < 0) {
        printf("Failed to start listening on the socket: %d (%s)\n", -errno,
                strerror(errno));
        ret = -errno;
        close();
        return ret;
    }

    return 0;
}

int SocketInterface::close()
{
    int ret;
    int errcode = 0;

    for (size_t i = 0; i < SOCK_INTERFACE_MAX_CLIENTS; i++) {
        if (m_clients[i].fd < 0) {
            continue;
        }
        ret = ::close(m_clients[i].fd);
        if (ret < 0) {
            printf("Failed to close client socket: %d (%s)\n", -errno,
                    strerror(errno));
            errcode = -errno;
        }
        m_clients[i].fd = -1;
        m_clients[i].message.reset();
    }

    ret = ::close(m_fd);
    if (ret < 0) {
        printf("Failed to close server socket: %d (%s)\n", -errno,
                strerror(errno));
        errcode = -errno;
    }
    m_fd = -1;

    return errcode;
}

void SocketInterface::receive()
{
    if (m_fd < 0) {
        return;
    }

    /* New clients connection */
    int new_client = accept(m_fd, NULL, 0);
    if (new_client < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            printf("Failed to accept connection: %d (%s)\n", -errno,
                    strerror(errno));
        }
    } else {
        int client_id = registerClient(new_client);
        if (client_id < 0) {
            printf("Failed to register new client: %d (%s)\n", client_id,
                    strerror(-client_id));
        }
    }

    /* Read from connected clients */
    for (size_t i = 0; i < SOCK_INTERFACE_MAX_CLIENTS; i++) {
        if (m_clients[i].fd < 0) {
            continue;
        }
        ssize_t size = recv(m_clients[i].fd, m_buffer, sizeof(m_buffer),
                MSG_DONTWAIT);
        if (size == 0) {
            freeClient(i);
        } else if (size < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                printf("Failed to read from client: %d (%s)\n", -errno,
                        strerror(errno));
                freeClient(i);
            }
        } else {
            for (ssize_t k = 0; k < size; k++) {
                int ll_ret = m_clients[i].message.append_byte(m_buffer[k]);
                if (ll_ret != LL_MSG_OK) {
                    printf("Invalid byte received from client #%lu (%u): %s\n",
                            i, m_buffer[k], LowLevelMessage::str_error(ll_ret));
                }
                if (m_clients[i].message.ready()) {
                    m_msg_queue.push(m_clients[i].message);
                    m_clients[i].message.reset();
                }
            }
        }
    }
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

void SocketInterface::sendMessage(const LowLevelMessage &message, int cid)
{
    if (m_fd < 0) {
        return;
    }

    int client_id;
    if (message.is_broadcast()) {
        client_id = cid;
    } else {
        client_id = message.get_client_id();
    }
    if (client_id < 0 || client_id >= SOCK_INTERFACE_MAX_CLIENTS) {
        printf("Invalid client ID (%d)\n", client_id);
        return;
    }
    if (m_clients[client_id].fd < 0) {
        return;
    }

    ssize_t size = message.get_frame_without_cid(m_buffer,
            SOCK_INTERFACE_BUFFER_SIZE);
    if (size < 0) {
        printf("LowLevelMessage::get_frame_without_cid: "
               "invalid message: %ld (%s)\n", size, strerror(-size));
        return;
    }

    ssize_t nb_bytes_sent = 0;
    while (nb_bytes_sent < size) {
        ssize_t ret = send(m_clients[client_id].fd, m_buffer + nb_bytes_sent,
                size - nb_bytes_sent, MSG_NOSIGNAL);
        if (ret < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                printf("Failed to send message on socket: %d (%s)\n", -errno,
                        strerror(errno));
                freeClient(client_id);
                return;
            }
        } else if (ret == 0) {
            printf("Failed to send message on socket (zero bytes written)\n");
            freeClient(client_id);
            return;
        } else {
            nb_bytes_sent += ret;
        }
    }
}

int SocketInterface::registerClient(int fd)
{
    if (fd < 0) {
        return -EINVAL;
    }

    for (size_t i = 0; i < SOCK_INTERFACE_MAX_CLIENTS; i++) {
        if (m_clients[i].fd < 0) {
            m_clients[i].fd = fd;
            return i;
        }
    }

    return -ENOMEM;
}

int SocketInterface::freeClient(size_t id)
{
    if (id >= SOCK_INTERFACE_MAX_CLIENTS) {
        return -EINVAL;
    }

    int fd = m_clients[id].fd;

    if (fd < 0) {
        return 0;
    }

    m_clients[id].fd = -1;
    m_clients[id].message.reset();

    return ::close(fd);
}

SocketInterface::Client::Client() :
        message(LL_MSG_SIDE_SOCKET)
{
    fd = -1;
}
