#include "SerialInterface.h"

#include <fcntl.h>
#include <cerrno>
#include <cstdio>
#include <termios.h>
#include <unistd.h>
#include <cstring>

SerialInterface::SerialInterface() :
    m_ll_msg(LL_MSG_SIDE_SERIAL)
{
    m_fd = -1;
}

SerialInterface::~SerialInterface() = default;

int SerialInterface::open(const char *port)
{
    int ret;
    struct termios serial_settings;

    /* Open serial port */
    m_fd = ::open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (m_fd < 0) {
        printf("Failed to open serial port: %d (%s)\n", -errno,
                strerror(errno));
        return -errno;
    }

    /* Read serial port settings */
    ret = tcgetattr(m_fd, &serial_settings);
    if (ret < 0) {
        printf("Failed to read port settings: %d (%s)\n", -errno,
                strerror(errno));
        ret = -errno;
        close();
        return ret;
    }

    /* Set RAW serial port */
    cfmakeraw(&serial_settings);

    /* Set other settings */
    serial_settings.c_cflag     &=  ~CSTOPB;            // one stop bit
    serial_settings.c_cflag     &=  ~CRTSCTS;           // no flow control
    serial_settings.c_cc[VMIN]   =  1;                  // minimum number of characters for non-canonical read
    serial_settings.c_cc[VTIME]  =  1;                  // timeout in deciseconds for non-canonical read
    serial_settings.c_cflag     |=  CREAD | CLOCAL;     // turn on READ & ignore ctrl lines

    /* Flush port */
    ret = tcflush(m_fd, TCIFLUSH);
    if (ret < 0) {
        printf("Failed to flush serial port: %d (%s)\n", -errno,
                strerror(errno));
        ret = -errno;
        close();
        return ret;
    }

    /* Apply settings to port */
    ret = tcsetattr(m_fd, TCSANOW, &serial_settings);
    if (ret < 0) {
        printf("Failed to apply settings to serial port: %d (%s)\n", -errno,
                strerror(errno));
        ret = -errno;
        close();
        return ret;
    }

    return 0;
}

int SerialInterface::close()
{
    int ret = ::close(m_fd);
    m_fd = -1;
    m_ll_msg.reset();
    if (ret < 0) {
        printf("Failed to close serial port: %d (%s)\n", -errno,
                strerror(errno));
        return -errno;
    }

    return 0;
}

int SerialInterface::receive()
{
    if (m_fd < 0) {
        return -ENOTCONN;
    }

    ssize_t size = read(m_fd, m_buffer, sizeof(m_buffer));
    if (size < 0) {
        if (errno == EAGAIN) {
            return 0;
        } else {
            printf("Failed to read from serial port: %d (%s)\n", -errno,
                    strerror(errno));
            return -errno;
        }
    } else if (size == 0) {
        printf("Reached EOF on serial port\n");
        return -ENOTCONN;
    }

    int ll_ret;
    for (ssize_t i = 0; i < size; i++) {
        ll_ret = m_ll_msg.append_byte(m_buffer[i]);
        if (ll_ret != LL_MSG_OK) {
            printf("Invalid byte received from serial (%u): %s\n",
                    m_buffer[i], LowLevelMessage::str_error(ll_ret));
        }
        if (m_ll_msg.ready()) {
            m_msg_queue.push(m_ll_msg);
            m_ll_msg.reset();
        }
    }

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
    if (m_fd < 0) {
        return -ENOTCONN;
    }

    ssize_t size = message.get_frame_with_cid(m_buffer,
            SERIAL_INTERFACE_BUFFER_SIZE);
    if (size < 0) {
        printf("LowLevelMessage::get_frame_with_cid: "
               "invalid message: %ld (%s)\n", size, strerror(-size));
        return 0;
    }

    ssize_t nb_bytes_sent = 0;
    while (nb_bytes_sent < size) {
        ssize_t ret = write(m_fd, m_buffer + nb_bytes_sent,
                size - nb_bytes_sent);
        if (ret < 0) {
            if (ret != EAGAIN) {
                printf("Failed to send message on serial: %d (%s)\n", -errno,
                        strerror(errno));
                return -errno;
            }
        } else if (ret == 0) {
            printf("Failed to send message on serial (zero bytes written)\n");
            return -ENOTCONN;
        } else {
            nb_bytes_sent += ret;
        }
    }

    return 0;
}
