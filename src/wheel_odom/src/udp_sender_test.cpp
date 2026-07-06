#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <unistd.h>

//ONLY EXISTS TO TEST ODOMPUBLISHER, RUN g++ udp_sender_test.cpp -o udp_sender && ./udp_sender IF YOU WANT TO TEST ODOMPUBLISHER


struct OdomPacket
{
    float x;
    float y;
    float theta;
    float linear_velocity;
    float angular_velocity;
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
    dest.sin_port = htons(12345);

    if (inet_pton(AF_INET, "127.0.0.1", &dest.sin_addr) != 1)
    {
        std::cerr << "Invalid IP address\n";
        return 1;
    }

    OdomPacket packet{};

    float t = 0.0f;

    while (true)
    {
        packet.x = t;
        packet.y = 2.0f * t;
        packet.theta = 0.1f * t;
        packet.linear_velocity = 1.5f;
        packet.angular_velocity = 0.25f;

        ssize_t sent = sendto(
            sock,
            &packet,
            sizeof(packet),
            0,
            reinterpret_cast<sockaddr*>(&dest),
            sizeof(dest));

        if (sent < 0)
        {
            perror("sendto");
        }
        else
        {
            std::cout
                << "Sent packet: "
                << "x=" << packet.x
                << " y=" << packet.y
                << " theta=" << packet.theta
                << std::endl;
        }

        t += 0.1f;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    close(sock);
    return 0;
}