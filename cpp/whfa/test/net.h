/**
 * @file test/net.h
 * @author Robert Griffith
 */
#pragma once

#include <cstdint>

namespace whfa::test
{

    /**
     * @brief test TCPConnection functionality
     *
     * @param port local port to test connection with self
     */
    void test_tcpconnection(uint16_t port);

    /**
     * @brief test TCPRAMFile
     *
     * @param port local port to use for connection
     */
    void test_tcpramfile(uint16_t port);

}