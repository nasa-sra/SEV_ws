/***************************************************************************//**
 *
 * File: SocketAddress.h
 *	Generic Software Communications Library
 *
 * Written by:
 * 	NASA, Johnson Space Center
 *
 * Acknowledgements:
 *   https://beej.us/guide/bgnet/pdf/bgnet_usl_c_1.pdf
 *
 * Copyright and License:
 *   Copyright and License information can be found in the LICENSE.md file
 *   distributed with this software.
 *
 ******************************************************************************/
#pragma once

#include <stdint.h>
#include <string>

/***************************************************************************//**
 *
 * This class is a wrapper to hide the platform dependent code and types
 * needed by this library from anything using this library.
 *
 * It's still being worked on so it's a little messy for now
 *
 ******************************************************************************/
class SocketAddress
{
	public:
        static const int16_t MAX_ADDR_SIZE = 128; /**< sizeof(struct sockaddr_storage) = 128 */

        static const uint8_t SA_TYPE_TCP;
        static const uint8_t SA_TYPE_UDP;

        static const uint8_t SA_FAMILY_UNSPECIFIED;
        static const uint8_t SA_FAMILY_IPV4;
        static const uint8_t SA_FAMILY_IPV6;

        SocketAddress(void);
        SocketAddress(const SocketAddress& rh);

        virtual ~SocketAddress(void);

        SocketAddress& operator= (const SocketAddress& rh);

        bool getHost(std::string& name, uint16_t& port);
        bool getHost(std::string& name, int32_t& port);

        bool loadAddress(int32_t type, int32_t family, const std::string& host, uint16_t port);
        bool loadAddress(int32_t type, int32_t family, const std::string& host, int32_t port);

        void list(void* data);

        int         sa_length;                    // Length of ai_addr
        uint8_t     sa_data[MAX_ADDR_SIZE];        // Binary address
};
