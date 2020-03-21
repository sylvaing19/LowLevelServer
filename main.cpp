#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cstring>

#include "MessageRouter.h"
#include "Pause.h"

/* Default settings */
#define DEFAULT_IP_ADDRESS "172.16.0.2"
#define DEFAULT_TCP_PORT 80
#define DEFAULT_SERIAL_PORT "/dev/ttyAMA0"
#define DEFAULT_PAUSE_IP_ADDRESS "127.0.0.1"
#define DEFAULT_PAUSE_TCP_PORT 23747
#define DEFAULT_PAUSE_TOKEN 19

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
    const char *ip_address = DEFAULT_IP_ADDRESS;
    uint16_t tcp_port = DEFAULT_TCP_PORT;
    const char *serial_port = DEFAULT_SERIAL_PORT;
    const char *pause_ip_address = DEFAULT_PAUSE_IP_ADDRESS;
    uint16_t pause_tcp_port = DEFAULT_PAUSE_TCP_PORT;
    uint8_t pause_token = DEFAULT_PAUSE_TOKEN;

    /* Read settings from arguments if provided */
    int opt;
    while ((opt = getopt(argc, argv, "a:p:s:b:q:t:")) != -1) {
        switch (opt) {
            case 'a':
                ip_address = optarg;
                break;
            case 'p': {
                unsigned long p = strtoul(optarg, nullptr, 10);
                if (p > 0 && p <= UINT16_MAX) {
                    tcp_port = p;
                } else {
                    fprintf(stderr, "Invalid TCP port provided\n");
                    exit(EXIT_FAILURE);
                }
                break;
            }
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
                    fprintf(stderr, "Invalid TCP port provided\n");
                    exit(EXIT_FAILURE);
                }
                break;
            }
            case 't': {
                unsigned long t = strtoul(optarg, nullptr, 10);
                if (t <= UINT8_MAX) {
                    pause_token = t;
                } else {
                    fprintf(stderr, "Invalid pause token provided\n");
                    exit(EXIT_FAILURE);
                }
                break;
            }
            default: /* '?' */
                fprintf(stderr, "Usage: %s [-a ip address] [-p tcp port] "
                                "[-s serial port] [-b pause ip address] "
                                "[-q pause tcp port] [-t pause token]\n",
                                argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    /* Instantiate router */
    MessageRouter message_router;
    message_router.setSocketAddress(ip_address);
    message_router.setSocketPort(tcp_port);
    message_router.setSerialPort(serial_port);

    /* Instantiate and open the pause socket */
    Pause pause;
    printf("Open pause socket at %s:%u with token %u\n", pause_ip_address,
            pause_tcp_port, pause_token);
    ret = pause.open(pause_ip_address, pause_tcp_port, pause_token);
    if (ret < 0) {
        fprintf(stderr, "Failed to open pause socket: %d (%s)\n", ret,
                strerror(-ret));
        exit(-ret);
    }

    printf("LowLevelServer started\n");

    signal(SIGINT, ctrl_c);
    while (!ctrl_c_pressed) {

        while (!message_router.isOpen() && !ctrl_c_pressed) {
            printf("Open message router between %s:%u and %s\n", ip_address,
                    tcp_port, serial_port);
            ret = message_router.open();
            if (ret < 0) {
                fprintf(stderr, "Failed to open message router: %d (%s)\n",
                        ret, strerror(-ret));
                usleep(1000000);
            }
        }

        while (!ctrl_c_pressed) {
            ret = message_router.communicate();
            if (ret < 0) {
                fprintf(stderr, "Communication error: %s\n", strerror(-ret));
                break;
            }

            if (pause.pauseRequested()) {
                printf("Close message router (pause requested)\n");
                message_router.close();
                pause.waitForResume();
                break;
            }

            usleep(1000);
        }
    }

    message_router.close();
    printf("LowLevelServer terminated\n");

    return 0;
}
