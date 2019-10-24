#include <stdexcept>
#include "LowLevelMessage.h"

LowLevelMessage::LowLevelMessage(LowLevelMessageSide msg_side)
{
    switch (msg_side) {
        case LL_MSG_SIDE_SOCKET:
            m_read_client_id = false;
            break;
        case LL_MSG_SIDE_SERIAL:
            m_read_client_id = true;
            break;
        default:
            throw std::invalid_argument("MSG_SIDE");
    }
    m_client_id = UNKNOWN_CLIENT_ID;
    m_read_state = HEADER;
    m_payload_length = 0;
    m_read_until_eof = false;
}

LowLevelMessage::~LowLevelMessage() = default;

int LowLevelMessage::append_byte(uint8_t byte)
{
    int ret = LL_MSG_OK;

    switch (m_read_state) {
        case HEADER:
            if (byte == HEADER_BYTE) {
                if (m_read_client_id) {
                    m_read_state = CLIENT;
                } else {
                    m_read_state = COMMAND;
                }
            } else {
                ret = LL_MSG_HEADER_ERR;
            }
            break;
        case CLIENT:
            if (byte <= BROADCAST_CLIENT_ID) {
                m_client_id = byte;
                m_read_state = COMMAND;
            } else {
                ret = LL_MSG_CLIENT_ID_ERR;
            }
            break;
        case COMMAND:
            m_frame.push_back(byte);
            m_read_state = LENGTH;
            break;
        case LENGTH:
            if (byte > INFO_FRAME_LENGTH) {
                ret = LL_MSG_SIZE_ERR;
            } else {
                m_frame.push_back(byte);
                if (byte == INFO_FRAME_LENGTH) {
                    m_payload_length = 0;
                    m_read_until_eof = true;
                    m_read_state = PAYLOAD;
                } else {
                    m_payload_length = byte;
                    m_read_until_eof = false;
                    if (m_payload_length == 0) {
                        m_read_state = FULL;
                    } else {
                        m_read_state = PAYLOAD;
                    }
                }
            }
            break;
        case PAYLOAD:
            m_frame.push_back(byte);
            if (m_read_until_eof) {
                if (byte == '\0') {
                    m_read_state = FULL;
                }
            } else if (m_frame.size() == m_payload_length + 2) {
                m_read_state = FULL;
            }
            break;
        case FULL:
            ret = LL_MSG_FULL;
            break;
        default:
            break;
    }

    if (ret != LL_MSG_OK && ret != LL_MSG_FULL) {
        reset();
    }

    return ret;
}

bool LowLevelMessage::ready() const
{
    return m_read_state == FULL;
}

void LowLevelMessage::reset()
{
    if (m_read_client_id) {
        m_client_id = UNKNOWN_CLIENT_ID;
    }
    m_frame.clear();
    m_read_state = HEADER;
    m_payload_length = 0;
    m_read_until_eof = false;
}

void LowLevelMessage::set_client_id(int client_id)
{
    if (!m_read_client_id) {
        m_client_id = client_id;
    }
}

int LowLevelMessage::get_client_id() const
{
    return m_client_id;
}

ssize_t LowLevelMessage::get_frame_with_cid(uint8_t *buf, size_t size) const
{
    if (buf == nullptr) {
        return -EFAULT;
    }
    if (m_client_id == UNKNOWN_CLIENT_ID) {
        return -EBADMSG;
    }
    if (size < 2) {
        return -EMSGSIZE;
    }

    buf[0] = HEADER_BYTE;
    buf[1] = (uint8_t)m_client_id;
    ssize_t ret = get_frame_body(buf + 2, size - 2);
    if (ret < 0) {
        return ret;
    } else {
        return ret + 2;
    }
}

ssize_t LowLevelMessage::get_frame_without_cid(uint8_t *buf, size_t size) const
{
    if (buf == nullptr) {
        return -EFAULT;
    }
    if (size < 1) {
        return -EMSGSIZE;
    }

    buf[0] = HEADER_BYTE;
    ssize_t ret = get_frame_body(buf + 1, size - 1);
    if (ret < 0) {
        return ret;
    } else {
        return ret + 1;
    }
}

ssize_t LowLevelMessage::get_frame_body(uint8_t *buf, size_t size) const
{
    if (!ready()) {
        return -EBADMSG;
    }
    if (size < m_frame.size()) {
        return -EMSGSIZE;
    }
    for (size_t i = 0; i < m_frame.size(); i++) {
        buf[i] = m_frame.at(i);
    }

    return m_frame.size();
}

const char *LowLevelMessage::str_error(int err_code)
{
    switch (err_code) {
        case LL_MSG_OK:
            return "LL_MSG OK";
        case LL_MSG_FULL:
            return "LL_MSG Full";
        case LL_MSG_HEADER_ERR:
            return "LL_MSG Invalid header";
        case LL_MSG_CLIENT_ID_ERR:
            return "LL_MSG Invalid client ID";
        case LL_MSG_SIZE_ERR:
            return "LL_MSG Invalid size";
        default:
            return "LL_MSG Unknown error";
    }
}
