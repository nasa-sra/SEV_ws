/***************************************************************************//**
 *
 * File: UdpSocket.h
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
#pragma once

#include "SocketAddress.h"

#include <stdint.h>
#include <string>

namespace gsc
{

/***************************************************************************//**
 *
 * The UDP Socket is a wrapper for socket communication using the Datagram
 * Protocol.
 *
 ******************************************************************************/
class UdpSocket
{
	public:
        UdpSocket(void);
        UdpSocket(uint16_t localPort);
        UdpSocket(const std::string& localAddress, uint16_t localPort);

        ~UdpSocket(void);

        // Delete the copy, move, and assignment methods because we are managing
        // a resource and don't want it duplicated
        UdpSocket(const UdpSocket& c) = delete;
        UdpSocket(UdpSocket&& c) = delete;
        UdpSocket& operator=(const UdpSocket& c) = delete;

        bool isReady(void);

        void setSendTimeout(float timeout);
        void setReceiveTimeout(float timeout);

        bool setLocalAddress(uint16_t local_port, const std::string& local_address = "");
        void getLocalAddress(SocketAddress& addr);
        void getLocalAddress(std::string &local_address, uint16_t& local_port);

        int32_t sendTo(const void *buffer, uint32_t buffer_len, SocketAddress& remote_address);


        int recvFrom(void *buffer, uint32_t buffer_len, SocketAddress& remote_address);
        int recvFrom(void *buffer, uint32_t buffer_len, SocketAddress& remote_address, double seconds);

        int32_t getSocketId(void);

        bool joinMulticastGroup(const std::string& group);
        bool setMulticastHops(uint8_t hops);

	private:
        int32_t socket_descriptor;
};

} // end namespace
