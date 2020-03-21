#pragma once

#include <cstdint>

class Pause
{
public:
    Pause();
    ~Pause();

    int open(const char *address_string, uint16_t server_port, uint8_t token);
    void close();
    bool pauseRequested();
    void waitForResume();

private:
    int m_server;
    int m_client;
    uint8_t m_token;
};
