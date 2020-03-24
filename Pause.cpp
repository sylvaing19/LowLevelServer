#include "Pause.h"

#include <cstdio>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

Pause::Pause()
{
    m_server = -1;
    m_client = -1;
    m_token = 0;
}

Pause::~Pause() = default;

int Pause::open(const char *address_string, uint16_t server_port, uint8_t token)
{
    int ret;

    // If socket already created, return error
    if (m_server >= 0) {
        printf("Socket interface already opened\n");
        return -EEXIST;
    }

    // Create the socket (non blocking)
    m_server = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
    if (m_server < 0) {
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
    ret = setsockopt(m_server, SOL_SOCKET, (SO_REUSEADDR | SO_REUSEPORT),
                     (const char *)(&option_value), sizeof(option_value));
    if (ret < 0) {
        printf("Failed to set socket options: %d (%s)\n", -errno,
                strerror(errno));
        ret = -errno;
        close();
        return ret;
    }

    // Bind socket to address
    ret = bind(m_server, (sockaddr*)(&server_address), sizeof(server_address));
    if (ret < 0) {
        printf("Failed to perform socket binding: %d (%s)\n", -errno,
                strerror(errno));
        ret = -errno;
        close();
        return ret;
    }

    // Start listening
    ret = listen(m_server, 1);
    if (ret < 0) {
        printf("Failed to start listening on the socket: %d (%s)\n", -errno,
                strerror(errno));
        ret = -errno;
        close();
        return ret;
    }

    m_token = token;
    return 0;
}

void Pause::close()
{
    if (m_client >= 0) {
        ::close(m_client);
    }
    m_client = -1;

    if (m_server >= 0) {
        ::close(m_server);
    }
    m_server = -1;

    m_token = 0;
}

bool Pause::pauseRequested()
{
    if (m_server < 0) {
        return false;
    }
    if (m_client < 0) {
        m_client = accept(m_server, NULL, 0);
    }
    if (m_client < 0) {
        return false;
    }

    uint8_t r_byte = 0;
    ssize_t size = recv(m_client, &r_byte, sizeof(r_byte), MSG_DONTWAIT);
    if (size == 0) {
        ::close(m_client);
        m_client = -1;
    } else if (size < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            ::close(m_client);
            m_client = -1;
        }
    } else {
        if (r_byte == m_token) {
            return true;
        }
    }

    return false;
}

void Pause::waitForResume()
{
    if (m_server < 0) {
        return;
    }

    while (m_client >= 0) {
        ssize_t ret = send(m_client, &m_token, sizeof(m_token), MSG_NOSIGNAL);
        if (ret < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                ::close(m_client);
                m_client = -1;
            }
        } else if (ret == 0) {
            ::close(m_client);
            m_client = -1;
        }

        if (m_client >= 0) {
            usleep(1000000);
        }
    }
}
