#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <sstream>

#include "MessageRouter.h"
#include "Pause.h"

/* Default settings */
#define DEFAULT_IP_ADDRESS "172.24.1.1"
#define DEFAULT_TCP_PORT 80
#define DEFAULT_SERIAL_PORT "/dev/ttyACM0"
#define DEFAULT_PAUSE_IP_ADDRESS "127.0.0.1"
#define DEFAULT_PAUSE_TCP_PORT 23747
#define DEFAULT_PAUSE_TOKEN 19
#define DEFAULT_LOG_FOLDER "."
#define DEFAULT_CONFIG_FILE "./low-level-server.conf"

/* Configuration keys */
#define CONF_KEY_IP_ADDRESS "IP_ADDRESS"
#define CONF_KEY_TCP_PORT "TCP_PORT"

/* Signal handler for CTRL+C */
bool ctrl_c_pressed = false;
void ctrl_c(int)
{
    ctrl_c_pressed = true;
}

int main(int argc, char *argv[])
{
    int ret;

    /* Init settings to default values */
    std::string ip_address = DEFAULT_IP_ADDRESS;
    uint16_t tcp_port = DEFAULT_TCP_PORT;
    const char *serial_port = DEFAULT_SERIAL_PORT;
    const char *pause_ip_address = DEFAULT_PAUSE_IP_ADDRESS;
    uint16_t pause_tcp_port = DEFAULT_PAUSE_TCP_PORT;
    uint8_t pause_token = DEFAULT_PAUSE_TOKEN;
    const char *log_folder = DEFAULT_LOG_FOLDER;
    const char *config_file_name = DEFAULT_CONFIG_FILE;

    /* Read settings from arguments if provided */
    int opt;
    while ((opt = getopt(argc, argv, "c:s:b:q:t:l:")) != -1) {
        switch (opt) {
            case 'c':
                config_file_name = optarg;
                break;
            case 's':
                serial_port = optarg;
                break;
            case 'b':
                pause_ip_address = optarg;
                break;
            case 'q': {
                unsigned long p = strtoul(optarg, nullptr, 10);
                if (p > 0 && p <= UINT16_MAX) {
                    pause_tcp_port = p;
                } else {
                    printf("Invalid TCP port provided\n");
                    exit(EXIT_FAILURE);
                }
                break;
            }
            case 't': {
                unsigned long t = strtoul(optarg, nullptr, 10);
                if (t <= UINT8_MAX) {
                    pause_token = t;
                } else {
                    printf("Invalid pause token provided\n");
                    exit(EXIT_FAILURE);
                }
                break;
            }
            case 'l':
                log_folder = optarg;
                break;
            default: /* '?' */
                printf("Usage: %s [-c config file] [-s serial port] "
                       "[-b pause ip address] [-q pause tcp port] "
                       "[-t pause token] [-l log folder]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    /* Instantiate router */
    MessageRouter message_router;
    message_router.setSerialPort(serial_port);

    /* Instantiate and open the pause socket */
    Pause pause;
    printf("Open pause socket at %s:%u with token %u\n", pause_ip_address,
            pause_tcp_port, pause_token);
    ret = pause.open(pause_ip_address, pause_tcp_port, pause_token);
    if (ret < 0) {
        printf("Failed to open pause socket: %d (%s)\n", ret, strerror(-ret));
        exit(-ret);
    }

    printf("LowLevelServer started with config file '%s' and log folder '%s'\n",
            config_file_name, log_folder);

    signal(SIGINT, ctrl_c);
    while (!ctrl_c_pressed) {

        while (!message_router.isOpen() && !ctrl_c_pressed) {

            /* Read config file to get IP address and TCP port */
            std::ifstream config_file(config_file_name);
            if(config_file.fail()) {
                printf("Failed to open config file: %s\n", config_file_name);
            } else {
                std::string line;
                while (std::getline(config_file, line)) {
                    std::istringstream is_line(line);
                    std::string key, value;
                    if (!std::getline(is_line, key, '=')) {
                        continue;
                    }
                    if (!std::getline(is_line, value)) {
                        continue;
                    }
                    if (strcmp(CONF_KEY_IP_ADDRESS, key.c_str()) == 0) {
                        ip_address = value;
                    } else if (strcmp(CONF_KEY_TCP_PORT, key.c_str()) == 0) {
                        unsigned long p = std::stoul(value);
                        if (p > 0 && p <= UINT16_MAX) {
                            tcp_port = p;
                        }
                    }
                }
            }
            config_file.close();

            message_router.setSocketAddress(ip_address.c_str());
            message_router.setSocketPort(tcp_port);
            printf("Open message router between %s:%u and %s\n",
                    ip_address.c_str(), tcp_port, serial_port);
            ret = message_router.open();
            if (ret < 0) {
                printf("Failed to open message router: %d (%s)\n",
                        ret, strerror(-ret));
                usleep(1000000);
            }
        }

        while (!ctrl_c_pressed) {
            ret = message_router.communicate();
            if (ret < 0) {
                printf("Communication error: %d (%s)\n", ret, strerror(-ret));
                usleep(1000000);
                break;
            }

            if (pause.pauseRequested()) {
                printf("Close message router (pause requested)\n");
                message_router.close();
                pause.waitForResume();
                break;
            }

            usleep(1000); // avoid using too much CPU time
        }
    }

    message_router.close();
    printf("LowLevelServer terminated\n");

    return 0;
}
