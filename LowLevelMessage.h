#pragma once

#include <cstdint>
#include <vector>
#include <sys/types.h>

#define UNKNOWN_CLIENT_ID (-1)
#define BROADCAST_CLIENT_ID (0xFE)
#define DATA_CHANNEL_COUNT 32

enum LowLevelMessageErr {
    LL_MSG_OK = 0,
    LL_MSG_FULL = 1,
    LL_MSG_HEADER_ERR = 2,
    LL_MSG_CLIENT_ID_ERR = 3,
    LL_MSG_SIZE_ERR = 4,
};

enum LowLevelMessageSide {
    LL_MSG_SIDE_SOCKET, /* Client ID not included in the input frame */
    LL_MSG_SIDE_SERIAL, /* Client ID included in the input frame */
};

class LowLevelMessage
{
public:
    explicit LowLevelMessage(LowLevelMessageSide msg_side);
    ~LowLevelMessage();

    int append_byte(uint8_t byte);
    bool ready() const;
    void reset();

    void set_client_id(int client_id);
    int get_client_id() const;

    ssize_t get_frame_with_cid(uint8_t *buf, size_t size) const;
    ssize_t get_frame_without_cid(uint8_t *buf, size_t size) const;

    static const char *str_error(int err_code);

private:
    ssize_t get_frame_body(uint8_t *buf, size_t size) const;

    bool m_read_client_id;
    int m_client_id;

    /* Contains all transmitted bytes except the header and the client id */
    std::vector<uint8_t> m_frame;

    enum ReadState {
        HEADER, CLIENT, COMMAND, LENGTH, PAYLOAD, FULL
    };
    ReadState m_read_state;
    unsigned int m_payload_length;
    bool m_read_until_eof;
};
