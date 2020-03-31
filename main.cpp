#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cstring>

#include "MessageRouter.h"
#include "Pause.h"

/* Default settings */
#define DEFAULT_TCP_PORT 2020
#define DEFAULT_SERIAL_PORT "/dev/ttyACM0"
#define DEFAULT_PAUSE_IP_ADDRESS "127.0.0.1"
#define DEFAULT_PAUSE_TCP_PORT 23747
#define DEFAULT_PAUSE_TOKEN 19
#define DEFAULT_LOG_FOLDER "."

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
    uint16_t tcp_port = DEFAULT_TCP_PORT;
    const char *serial_port = DEFAULT_SERIAL_PORT;
    const char *pause_ip_address = DEFAULT_PAUSE_IP_ADDRESS;
    uint16_t pause_tcp_port = DEFAULT_PAUSE_TCP_PORT;
    uint8_t pause_token = DEFAULT_PAUSE_TOKEN;
    const char *log_folder = DEFAULT_LOG_FOLDER;

    /* Read settings from arguments if provided */
    int opt;
    while ((opt = getopt(argc, argv, "s:p:b:q:t:l:")) != -1) {
        switch (opt) {
            case 's':
                serial_port = optarg;
                break;
            case 'p': {
                unsigned long p = strtoul(optarg, nullptr, 10);
                if (p > 0 && p <= UINT16_MAX) {
                    tcp_port = p;
                } else {
                    printf("Invalid TCP port provided\n");
                    exit(EXIT_FAILURE);
                }
                break;
            }
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
    message_router.setSocketPort(tcp_port);

    /* Instantiate and open the pause socket */
    Pause pause;
    printf("Open pause socket at %s:%u with token %u\n", pause_ip_address,
            pause_tcp_port, pause_token);
    ret = pause.open(pause_ip_address, pause_tcp_port, pause_token);
    if (ret < 0) {
        printf("Failed to open pause socket: %d (%s)\n", ret, strerror(-ret));
        exit(-ret);
    }

    printf("LowLevelServer started with log folder '%s'\n", log_folder);

    signal(SIGINT, ctrl_c);
    while (!ctrl_c_pressed) {

        while (!message_router.isOpen() && !ctrl_c_pressed) {
            printf("Open message router with port %u and serial %s\n",
                    tcp_port, serial_port);
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
