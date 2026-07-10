/***************************************************************************//**
 *
 * File: SocketAddress.cpp
 *	Generic Software Communications Library
 *
 * Written by:
 * 	NASA, Johnson Space Center
 *
 * Acknowledgements:
 *
 * Copyright and License:
 *   Copyright and License information can be found in the LICENSE.md file
 *   distributed with this software.
 *
 ******************************************************************************/
#include "SocketAddress.h"

#if defined(_WIN32)
#include <Ws2tcpip.h>
#include <winsock.h>

typedef int socklen_t;
typedef char rawdata_t;       // Type used for raw data on this platform

#else
#include <sys/types.h>       // For data types
#include <sys/socket.h>      // For socket(), connect(), send(), and recv()
#include <netdb.h>           // For gethostbyname()
#include <arpa/inet.h>       // For inet_addr()
#include <unistd.h>          // For close()
#include <netinet/in.h>      // For sockaddr_in
#include <string.h>
typedef void rawdata_t;       // Type used for raw data on this platform
#endif

/***************************************************************************//**
 *
 ******************************************************************************/
const uint8_t SocketAddress::SA_TYPE_TCP = SOCK_STREAM;
const uint8_t SocketAddress::SA_TYPE_UDP = SOCK_DGRAM;

const uint8_t SocketAddress::SA_FAMILY_UNSPECIFIED = AF_UNSPEC;
const uint8_t SocketAddress::SA_FAMILY_IPV4        = AF_INET;
const uint8_t SocketAddress::SA_FAMILY_IPV6        = AF_INET6;

/***************************************************************************//**
 *
 ******************************************************************************/
SocketAddress::SocketAddress(void)
{
    sa_length = 0;             // Length of ai_addr
    memset(sa_data, 0, SocketAddress::MAX_ADDR_SIZE);
}

/***************************************************************************//**
 *
 ******************************************************************************/
SocketAddress::SocketAddress(const SocketAddress& rh)
{
    sa_length = rh.sa_length;
    memset(sa_data, 0, SocketAddress::MAX_ADDR_SIZE);
    memcpy(sa_data, rh.sa_data, sa_length);
}

/***************************************************************************//**
 *
 ******************************************************************************/
SocketAddress::~SocketAddress(void)
{
}

/***************************************************************************//**
 *
 ******************************************************************************/
SocketAddress& SocketAddress::operator= (const SocketAddress &rh)
{
    if (this == &rh)
    {
        return *this;
    }

    sa_length = rh.sa_length;
    memset(sa_data, 0, SocketAddress::MAX_ADDR_SIZE);
    memcpy(sa_data, rh.sa_data, sa_length);

    return *this;
}

/***************************************************************************//**
 *
 * @brief DON'T USE THIS
 *
 * This was put in place to be backward compatible, ports should be
 * of type uint16_t.
 *
 * This method calls the getHost(string, uint16_t) method then sets the
 * the provided port number as an int32_t
 *
 * @returns true on success, false otherwise
 *
 ******************************************************************************/
bool SocketAddress::getHost(std::string& name, int32_t &port)
{
    uint16_t tmp_port;
    if (!getHost(name, tmp_port))
    {
        return false;
    }
    port = tmp_port;
    return true;
}

/***************************************************************************//**
 *
 * Get the host name and port from this class.
 *
 * This will set the provided references based on the data this instance
 * is holding, this might not match what was sent to the loadAddress method
 * but it will represent the address.
 *
 * @param   name    a reference to the variable in which the name should be stored
 *
 * @param   port    a reference to the variable in which the port should be
 *                  stored, the port will be in host byte order
 *
 * @returns true on success, false otherwise
 *
 ******************************************************************************/
bool SocketAddress::getHost(std::string& name, uint16_t& port)
{
	// The first 2 bytes of both IPV4 and IPV6 sockaddr are the family
	// so start with IPV4 structure for checking the family
    struct sockaddr_in *ipv4 = (struct sockaddr_in *)sa_data;
    if (ipv4->sin_family == AF_INET) // IPv4
    {
        char ipstr[INET6_ADDRSTRLEN];
        inet_ntop(ipv4->sin_family, &(ipv4->sin_addr), ipstr, sizeof ipstr);
        name = std::string(ipstr);
        port = ntohs(ipv4->sin_port);
    }
    else if (ipv4->sin_family == AF_INET6)// IPv6
    {
        char ipstr[INET6_ADDRSTRLEN];
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)sa_data;
        inet_ntop(ipv6->sin6_family, &(ipv6->sin6_addr), ipstr, sizeof ipstr);
        name = std::string(ipstr);
        port = ntohs(ipv6->sin6_port);
    }
    else
    {
        name = "unknown";
        port = 0;
        return false;
    }

    return true;
}

/***************************************************************************//**
 *
 * @brief DON'T USE THIS
 *
 * This was put in place to be backward compatible, ports should be
 * of type uint16_t.
 *
 * This method cast the port to the correct type then calls the
 * loadAddress(int32_t, int32_t, string, uint16_t) method
 *
 * @returns true on success, false otherwise
 *
 ******************************************************************************/
bool SocketAddress::loadAddress(int32_t type, int32_t family, const std::string& host, int32_t port)
{
    return loadAddress(type, family, host, (uint16_t)port);
}

/***************************************************************************//**
 *
 * Load this instance with an acceptable address for the provided arguments.
 *
 * This will search the system and network to see if an appropriate address
 * can be found, if one is, it will be loaded into this instance.
 *
 * @param type  the type of port SA_TYPE_UDP, SA_TYPE_TCP
 * @param family the protocol family for the socket SA_FAMILY_IPV4, SA_FAMILY_IPV6
 * @param host  the local address for the name
 * @param port  the local port IN HOST BYTE ORDER
 *
 * @returns true on success, false otherwise
 *
 ******************************************************************************/
bool SocketAddress::loadAddress(int32_t type, int32_t family, const std::string& host, uint16_t port)
{
    struct addrinfo  hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = family;
    hints.ai_socktype = type;

    const char *node_name;
    if (host.length() > 0)
    {
        node_name = host.c_str();
    }
    else // use any local address
    {
        node_name = nullptr;
        hints.ai_flags = AI_PASSIVE;
    }

    hints.ai_flags |= AI_NUMERICSERV;

    char node_service[8];
    memset(node_service, 0, 8);

    sprintf(node_service, "%d", port);
    struct addrinfo* res;
    int addr_status = getaddrinfo(node_name, node_service, &hints, &res);
    if (addr_status == 0)
    {
//        list(res); // helpful when debugging

        sa_length = static_cast<int>(res->ai_addrlen);
        memset(sa_data, 0, MAX_ADDR_SIZE);
        memcpy(sa_data, res->ai_addr, sa_length);

        freeaddrinfo(res); // free the linked list
    }
    else
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addr_status));
        sa_length = 0;
        memset(sa_data, 0, SocketAddress::MAX_ADDR_SIZE);
        return false;
    }
    return true;
}

/***************************************************************************//**
 *
 * This is for debugging, it will list all of the results from a call to
 * getaddrinfo()
 *
 ******************************************************************************/
void SocketAddress::list(void* data)
{
    struct addrinfo* adder_list = (struct addrinfo*)data;

    char ipstr[INET6_ADDRSTRLEN];

    printf(" -- address list --\n");
    struct addrinfo* p;
    for(p = adder_list;p != NULL; p = p->ai_next)
    {
        void *addr;

        // get the pointer to the address itself,
        // different fields in IPv4 and IPv6:
        if (p->ai_family == AF_INET)
        { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
            inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
            printf("IPv4: %s:%d\n", ipstr, ntohs(ipv4->sin_port));
        }
        else
        { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
            printf("IPv6: %s:%d\n", ipstr, ntohs(ipv6->sin6_port));
        }
    }
    printf(" ------------------\n");
}
