#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <unistd.h>

//ONLY EXISTS TO TEST ODOMPUBLISHER, RUN g++ udp_sender_test.cpp -o udp_sender && ./udp_sender IF YOU WANT TO TEST ODOMPUBLISHER


struct OdomPacket
{
    float data[17];
};

int main()
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0)
    {
        perror("socket");
        return 1;
    }

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(8324);

    if (inet_pton(AF_INET, "127.0.0.1", &dest.sin_addr) != 1)
    {
        std::cerr << "Invalid IP address\n";
        return 1;
    }

    OdomPacket packet{};

    float t = 0.0f;

    while (true)
    {
        OdomPacket packet{};

        float t = 0.0f;

        while (true)
        {
            packet.data[0] = t;          // x
            packet.data[1] = 2.0f * t;   // y
            packet.data[2] = 0.0f;       // z

            packet.data[3] = 0.0f;       // roll
            packet.data[4] = 0.0f;       // pitch
            packet.data[5] = 0.1f * t;   // yaw

            packet.data[6] = 1.5f;      // x velocity
            packet.data[7] = 0.0f;      // y velocity
            packet.data[8] = 0.0f;      // z velocity

            packet.data[9]  = 0.0f;     // roll velocity
            packet.data[10] = 0.0f;     // pitch velocity
            packet.data[11] = 0.25f;    // yaw velocity

            // unused values
            packet.data[12] = 0.0f;
            packet.data[13] = 0.0f;
            packet.data[14] = 0.0f;
            packet.data[15] = 0.0f;
            packet.data[16] = 0.0f;

            ssize_t sent = sendto(
                sock,
                &packet,
                sizeof(packet),
                0,
                reinterpret_cast<sockaddr*>(&dest),
                sizeof(dest)
            );

            std::cout << "Sent " << sent << " bytes\n";

            t += 0.1f;

            std::this_thread::sleep_for(
                std::chrono::milliseconds(100)
            );
        }
    }

    close(sock);
    return 0;
}