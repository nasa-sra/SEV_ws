/***************************************************************************//**
 *
 * File: UdpSocket.cpp
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
#include "UdpSocket.h"

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

#include <errno.h>             // For errno

namespace gsc
{

/***************************************************************************//**
 *
 * Create a default instance of a UDP socket
 *
 * This socket will use a "randomly" selected port number and the "ANY" address.
 * So on computers with multiple network cards the used address will be selected
 * according to the system defaults.
 *
 *
 ******************************************************************************/
UdpSocket::UdpSocket(void)
{
    socket_descriptor = -1;

#if defined(_WIN32)
    // NOTE: It's ok to call this multiple times. Windows will just
    // bump a reference count for each call.
    WSADATA wsaData;
    // Load WinSock DLL, Request WinSock v2.0
    int wsa_ret = WSAStartup( MAKEWORD(2, 0), &wsaData);
    if (wsa_ret != 0)
    {
        return;
    }
#endif

    // Make a new socket
    socket_descriptor = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

/***************************************************************************//**
 *
 * Create a UDP socket using the specified port number
 *
 * This will set the socket to use the specified port and the "ANY" address.
 * So on computers with multiple network cards the used address will be selected
 * according to the system defaults when sending messages.
 *
 ******************************************************************************/
UdpSocket::UdpSocket(uint16_t local_port)
    : UdpSocket()
{
    setLocalAddress(local_port);
}

/***************************************************************************//**
 *
 * Create a UDP socket using the specified address and port, this is useful for
 * computers with multiple network interface.
 *
 ******************************************************************************/
UdpSocket::UdpSocket(const std::string &local_address, uint16_t local_port)
    : UdpSocket()
{
    setLocalAddress(local_port, local_address);
}

/***************************************************************************//**
 *
 * Release resources used by this instance, this includes closing the socket.
 *
 ******************************************************************************/
UdpSocket::~UdpSocket(void)
{
    if (socket_descriptor >= 0)
    {
#if defined(_WIN32)
        closesocket(socket_descriptor);
#else
        close(socket_descriptor);
#endif
        socket_descriptor = -1;
    }

#if defined(_WIN32)
    // Each call to WSACleanup will decrement a reference
    // count until there are no more references. When it
    // reaches the end, then it does the cleanup.
    WSACleanup();
#endif
}

/***************************************************************************//**
 *
 * Because the constructor can't return error indicators, this method should
 * be called after creating a socket to make sure the socket was created
 * correctly.
 *
 * @return  true if the socket is ready to be used.
 *
 ******************************************************************************/
bool UdpSocket::isReady(void)
{
    return (socket_descriptor >= 0);
}

/***************************************************************************//**
 *
 * Get a unique identifier for this socket.
 *
 * At this time the socket descriptor is returned but it should not be used
 * as a raw descriptor because in the future the returned value might not
 * be the descriptor.
 *
 * @return  The ID for this socket.
 *
 ******************************************************************************/
int32_t UdpSocket::getSocketId(void)
{
    return socket_descriptor;
}

/***************************************************************************//**
 *
 * This can be used to specify the local address and port that should be used
 * by this socket.
 *
 * @param   local_port  the port that this socket should use.
 *
 * @param   local_address   the string representation of the local address that
 *                          should be used, default is an empty string which
 *                          will be treated as INADDR_ANY -- or you don't care
 *
 * @return true if the address was set correctly
 *
 ******************************************************************************/
bool UdpSocket::setLocalAddress(uint16_t local_port, const std::string &local_address)
{
    if (socket_descriptor < 0)
    {
        return false;
    }

    SocketAddress local_addr;
    local_addr.loadAddress(SocketAddress::SA_TYPE_UDP, SocketAddress::SA_FAMILY_IPV4,
        local_address, local_port);

    if (bind(socket_descriptor, (sockaddr*)local_addr.sa_data, (socklen_t) local_addr.sa_length) < 0)
    {
        return false;
    }

    return true;
}

/***************************************************************************//**
 *
 * Get the address of the underlying socket descriptor
 *
 * @param[out] local_address  the string representation of the local address
 *
 * @param[out] local_port  the port of the local address
 *
 ******************************************************************************/
void UdpSocket::getLocalAddress(std::string& local_address, uint16_t& local_port)
{
    if (socket_descriptor >= 0)
    {
        struct sockaddr_in local_addr;
        socklen_t len = sizeof(local_addr);

        // Get the local address and port of the bound socket
        getsockname(socket_descriptor, (struct sockaddr *)&local_addr, &len);

        // Convert the IP address to a human-readable string
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(local_addr.sin_addr), ip_str, INET_ADDRSTRLEN);

        local_address = ip_str;
        local_port = ntohs(local_addr.sin_port);
    }
}

/***************************************************************************//**
 *
 * @brief Get the address of the underlying socket descriptor
 *
 * @param   addr    a reference to the variable in which the address should be
 *                  stored.
 *
 ******************************************************************************/
void UdpSocket::getLocalAddress(SocketAddress& addr)
{
    if (socket_descriptor >= 0)
    {
        struct sockaddr_in local_addr;
        socklen_t len = sizeof(local_addr);

        // Get the local address and port of the bound socket
        getsockname(socket_descriptor, (struct sockaddr *)&local_addr, &len);

        // Convert the IP address to a human-readable string
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(local_addr.sin_addr), ip_str, INET_ADDRSTRLEN);

        uint16_t swapped_port = ntohs(local_addr.sin_port);
        addr.loadAddress(SocketAddress::SA_TYPE_UDP, local_addr.sin_family, ip_str, swapped_port);
    }
}

/***************************************************************************//**
 *
 * Set this socket to be non-blocking for receives.
 *
 * This class provides two recvFrom methods, the one that does not take
 * a timeout value will block by default but can be set to non-blocking by
 * calling this method with the appropriate timeout value. If you need your
 * timeout to be variable you should use the recvFrom that takes a timeout
 * value. This method does not impact the recvFrom that takes a timeout value,
 * that method will always use the value passed to it for timing.
 *
 * @param   seconds a floating point representation of the number of seconds
 *                  that a call to recvFrom should wait before returning
 *                  without any data.
 *
 ******************************************************************************/
void UdpSocket::setReceiveTimeout(float seconds)
{
#if defined(_WIN32)
    DWORD timeout = (DWORD)(seconds * 1000);
    setsockopt(socket_descriptor, SOL_SOCKET, SO_RCVTIMEO,(char *)&timeout, sizeof(timeout));

#else
    struct timeval tv;
    tv.tv_sec = (int)seconds;
    tv.tv_usec = (int)((seconds - tv.tv_sec) * 1000000);
    setsockopt(socket_descriptor, SOL_SOCKET, SO_RCVTIMEO,(void *)&tv, sizeof(tv));
#endif
}

/***************************************************************************//**
 *
 * Set the socket to be non-blocking for sends
 *
 * It would be rare that a UDP socket would block on a send, but it is
 * possible on some systems. Setting a Send timeout means that a send
 * will return after the specified amount of time even it the message
 * was not sent.
 *
 * @param   seconds a floating point seconds that represents how long
 *                  an individual send should try to send the message before
 *                  failing do to a timeout.
 *
 ******************************************************************************/
void UdpSocket::setSendTimeout(float seconds)
{
#if defined(_WIN32)
    DWORD timeout = (DWORD)(seconds * 1000);
    setsockopt(socket_descriptor, SOL_SOCKET, SO_SNDTIMEO,(char *)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = (int)seconds;
    tv.tv_usec = (int)((seconds - tv.tv_sec) * 1000000);
    setsockopt(socket_descriptor, SOL_SOCKET, SO_SNDTIMEO,(void *)&tv, sizeof(tv));
#endif
}

/***************************************************************************//**
 *
 * Send the specified message to the specified destination
 *
 * @param   buffer          a pointer to the start of the buffer that is to be sent
 * @param   buffer_len      the number of bytes that should be sent from the buffer
 * @param   remote_address  the network address to which the message should be sent
 *
 * @return  the number of bytes sent, or a negative number that represents
 *          an error condition (-1 * errno) or (-1 * WSAGetLastError())
 *
 ******************************************************************************/
int32_t UdpSocket::sendTo(const void *buffer, uint32_t buffer_len, SocketAddress& remote_address)
{
    if (sendto(socket_descriptor, (rawdata_t *) buffer, buffer_len, 0,
        (sockaddr *)remote_address.sa_data, (socklen_t) remote_address.sa_length) != (int32_t)buffer_len)
    {
#if defined(_WIN32)
        return (-1 * WSAGetLastError());
#else
        return (-1 * errno);
#endif
    }

    return buffer_len;
}

/***************************************************************************//**
 *
 * Receive a message
 *
 * This will read one message from the UDP port and save it in the provided
 * buffer.
 *
 * This version of recvFrom uses select to wait for a to become available
 * so it is using a select timeout and does not depend on the socket timeout
 * that may or may not have been set.
 *
 * @param      buffer          the buffer into which the message will be read
 * @param      buffer_len      the maximum number of bytes that should be read
 *                             into the buffer
 * @param[out] remote_address  the address from which the message came
 * @param      seconds         the number of seconds or fractional part of
 *                             a second that you are willing to wait for a new
 *                             message
 *
 * @return  the number of bytes received, if negative an error was detected
 *
 ******************************************************************************/
int UdpSocket::recvFrom(void *buffer, uint32_t buffer_len, SocketAddress& remote_address, double seconds)
{
    fd_set fds;
    int n;
    struct timeval tv;

    // Set up the file descriptor set.
    FD_ZERO(&fds) ;
    FD_SET(socket_descriptor, &fds) ;

    // Set up the struct timeval for the timeout.
    tv.tv_sec = (uint32_t)seconds ;
    tv.tv_usec = (uint32_t)((seconds - tv.tv_sec) * 1000000);

    // Wait until timeout or data received.
    // select requires the number of file descriptors plus 1
    n = select ( socket_descriptor + 1, &fds, NULL, NULL, &tv ) ;
    if( n == 0)
    {
        return 0;
    }
    else if(n < 0 )
    {
        return -1001;
    }

    remote_address.sa_length = SocketAddress::MAX_ADDR_SIZE;
    socklen_t len = (socklen_t) remote_address.sa_length;

    n = ::recvfrom(socket_descriptor, (rawdata_t*)buffer, buffer_len, 0,
              (sockaddr*)(remote_address.sa_data), &len);

    remote_address.sa_length = static_cast<int>(len);

    if (n < 0)
    {
#if defined(_WIN32)
        switch(WSAGetLastError())
        {
            case 0:
            case WSAEWOULDBLOCK:
            case WSAETIMEDOUT:
                return(0);
            default:
                fprintf(stderr, "Socket Error, errno = %d\n", WSAGetLastError());
                return (-1 * WSAGetLastError());
        }
#else
        switch(errno)
        {
            case 0:
            case EAGAIN:
            case ETIMEDOUT:
                return(0);
            default:
                fprintf(stderr, "Socket Error, errno = %d\n", errno);
                return (-1 * errno);
        }
#endif
    }

    return(n);
}

/***************************************************************************//**
 *
 * Receive a message
 *
 * This will read one message from the UDP port and save it in the provided
 * buffer.
 *
 * This version of recvFrom uses the socket timeout (which may have been set with
 * setReceiveTimeout) to wait for the message. By default the socket is blocking
 * and will not return until a message is received. If setReceiveTimeout was called
 * this call wait at most the specified timeout value before returning.
 *
 * @param   buffer          the buffer into which the message will be read
 * @param   buffer_len      the maximum number of bytes that should be read into the buffer
 * @param   remote_address  the address from which the message came will be written to this
 *
 * @return  the number of bytes received, if negative an error was detected
 *
 ******************************************************************************/
int UdpSocket::recvFrom(void *buffer, uint32_t buffer_len, SocketAddress& remote_address)
{
    int rtn;

    remote_address.sa_length = SocketAddress::MAX_ADDR_SIZE;
    socklen_t len = (socklen_t) remote_address.sa_length;
    rtn = ::recvfrom(socket_descriptor, (rawdata_t*)buffer, buffer_len, 0,
              (sockaddr*)(remote_address.sa_data), &len);

    if (rtn < 0)
    {
#if defined(_WIN32)
        switch(WSAGetLastError())
        {
            case 0:
            case WSAEWOULDBLOCK:
            case WSAETIMEDOUT:
                return(0);
            default:
                fprintf(stderr, "Socket Error, errno = %d\n", WSAGetLastError());
                return (-1 * WSAGetLastError());
        }
#else
        switch(errno)
        {
            case 0:
            case EAGAIN:
            case ETIMEDOUT:
                return(0);
            default:
                fprintf(stderr, "Socket Error, errno = %d\n", errno);
                return (-1 * errno);
        }
#endif
    }

    return rtn;
}

/***************************************************************************//**
 *
 * Connect a socket to listen for messages intended for a multicast group.
 *
 * Multicast groups look like IP addresses between 224.0.0.0 and 239.255.255.255
 * There is a way to resever multicast address for specific uses so it is
 * suggested that unregistered uses should stay in the 239.0.0.0 to 239.255.255.255
 * range to avoid conflicting with registered address. It is unlikely that you
 * will run into a conflict in the wider range but it if you take the chance
 * and do hit a conflict -- you have been warned.
 *
 * @param   group   the group to which the socket should be added to
 *
 * @return true if joined to the group
 *
 ******************************************************************************/
bool UdpSocket::joinMulticastGroup(const std::string& group)
{
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(group.c_str());
    if (mreq.imr_multiaddr.s_addr == INADDR_NONE)
    {
        // Invalid string passed for multicast group
        return false;
    }
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    return
        (setsockopt(socket_descriptor, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &mreq, sizeof(mreq)) >= 0);
}

/***************************************************************************//**
 *
 * Applications sending to a multicast group may want to set the number of
 * hops or (Time To Live (TTL)) for the messages they are sending, this can be
 * used to limit how many network devices (routers/switches) the message
 * pass through to reduce the potential reach of the message.
 *
 * @param hops  the number of network hops outgoing messages are allowed to
 *              make
 *
 * @return true if the hops was set correctly
 *
 ******************************************************************************/
bool UdpSocket::setMulticastHops(uint8_t hops)
{
    return
        (setsockopt(socket_descriptor, IPPROTO_IP, IP_MULTICAST_TTL, (const char*)&hops, sizeof(uint8_t)) >= 0);
}

} // end namespace
