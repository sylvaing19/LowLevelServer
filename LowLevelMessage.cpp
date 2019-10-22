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

int LowLevelMessage::get_frame(std::vector<uint8_t> &frame) const
{
    return 0;
}
