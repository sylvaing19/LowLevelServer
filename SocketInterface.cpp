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
        perror("Socket interface already opened");
        return -EEXIST;
    }

    // Create the socket (non blocking)
    m_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
    if (m_fd < 0) {
        perror("Failed to create socket");
        return -errno;
    }

    // Address and port conversion (string to binary data)
    sockaddr_in server_address = {};
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);

    ret = inet_pton(AF_INET, address_string, &server_address.sin_addr);
    if (ret < 0) {
        perror("Failed to convert IP address string");
        return -errno;
    } else if (ret != 1) {
        perror("Invalid IP address string provided");
        return -EINVAL;
    }

    // Set option: reusable addresses and ports
    int option_value = 1;
    ret = setsockopt(m_fd, SOL_SOCKET, (SO_REUSEADDR | SO_REUSEPORT),
            (const char *)(&option_value), sizeof(option_value));
    if (ret < 0) {
        perror("Failed to set socket options");
        return -errno;
    }

    // Bind socket to address
    ret = bind(m_fd, (sockaddr*)(&server_address), sizeof(server_address));
    if (ret < 0) {
        perror("Failed to perform socket binding");
        return -errno;
    }

    // Start listening
    ret = listen(m_fd, SOCK_INTERFACE_MAX_CLIENTS);
    if (ret < 0) {
        perror("Failed to start listening on the socket");
        return -errno;
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
            perror("Failed to close client socket");
            errcode = -errno;
        }
        m_clients[i].fd = -1;
        m_clients[i].message.reset();
    }

    ret = ::close(m_fd);
    if (ret < 0) {
        perror("Failed to close server socket");
        errcode = -errno;
    }
    m_fd = -1;

    return errcode;
}

int SocketInterface::receive()
{
    if (m_fd < 0) {
        return -ENOTCONN;
    }

    int ret = 0;

    /* New clients connection */
    int new_client = accept(m_fd, NULL, 0);
    if (new_client < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("Failed to accept connection");
            ret = -errno;
        }
    } else {
        int client_id = registerClient(new_client);
        if (client_id < 0) {
            fprintf(stderr, "Failed to register new client: %s\n",
                    strerror(-client_id));
            ret = client_id;
        }
    }

    /* Read from connected clients */
    for (size_t i = 0; i < SOCK_INTERFACE_MAX_CLIENTS; i++) {
        if (m_clients[i].fd < 0) {
            continue;
        }
        ssize_t size = recv(m_clients[i].fd, m_buffer, sizeof(m_buffer), 0);
        if (size == 0) {
            ret = freeClient(i);
        } else if (size < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("Failed to read from client");
                ret = -errno;
            }
        } else {
            for (ssize_t k = 0; k < size; k++) {
                int ll_ret = m_clients[i].message.append_byte(m_buffer[k]);
                if (m_clients[i].message.ready()) {
                    m_msg_queue.push(m_clients[i].message);
                    m_clients[i].message.reset();
                }

                if (ll_ret != LL_MSG_OK) {
                    fprintf(stderr, "Invalid byte received (%u): %s\n",
                            m_buffer[k], LowLevelMessage::str_error(ll_ret));
                    ret = -EBADMSG;
                }
            }
        }
    }

    return ret;
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
    if (m_fd < 0) {
        return -ENOTCONN;
    }

    int client_id = message.get_client_id();
    if (client_id < 0 || client_id >= SOCK_INTERFACE_MAX_CLIENTS) {
        return -EINVAL;
    }
    if (m_clients[client_id].fd < 0) {
        return -ENOTCONN;
    }

    ssize_t size = message.get_frame_without_cid(m_buffer,
            SOCK_INTERFACE_BUFFER_SIZE);
    if (size < 0) {
        return size;
    }

    ssize_t nb_bytes_sent = 0;
    while (nb_bytes_sent < size) {
        ssize_t ret = send(m_clients[client_id].fd, m_buffer, size,
                MSG_NOSIGNAL);
        if (ret < 0) {
            if (errno == EPIPE || errno == ECONNRESET) {
                freeClient(client_id);
                return -errno;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("Failed to send message");
                freeClient(client_id);
                return -errno;
            }
        } else if (ret == 0) {
            fprintf(stderr, "Failed to send message (zero bytes written)\n");
            freeClient(client_id);
            return -ENOTCONN;
        } else {
            nb_bytes_sent += ret;
        }
    }

    return 0;
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
