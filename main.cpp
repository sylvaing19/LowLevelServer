//
// Created by Sylvain on 18/10/2019.
//

#include <iostream>
#include "LowLevelMessage.h"
#include "SocketInterface.h"
#include "SerialInterface.h"

int main(int argc, const char * argv[])
{
    LowLevelMessage ll_msg(LL_MSG_SIDE_SOCKET);
    SocketInterface tcp_interface;
    SerialInterface tty_interface;

    tcp_interface.open("127.0.0.1", 2223);
    tcp_interface.close();

    tty_interface.open("COM3");
    tty_interface.close();

    std::cout << "Lol world" << std::endl;
    return 0;
}
