/**
 * @file net/tcpconnection.h
 * @author Robert Griffith
 */
#pragma once

#include <cstdint>
#include <mutex>

namespace whfa::net
{

    /**
     * @class whfa::net::TCPConnection
     * @brief class for simple sending and receiving over a TCP connection
     *
     * connect/accept/close are exclusively threadsafe
     * all recv/send are exclusively threadsafe
     * connect/accept/close will cause blocking recv/send to fail
     * simple blocking/nonblocking and poll timeout functionality
     * expecting full sends and receives
     */
    class TCPConnection
    {
    public:
        /**
         * @brief constructor
         *
         * @param fd file descriptor of optional existing connection
         */
        TCPConnection(int fd = -1);

        /**
         * @brief destructor that closes any current connection
         */
        virtual ~TCPConnection();

        /**
         * @brief opens connection as a client, blocking
         *
         * @param addr IP address to connect to
         * @param port port to connect to
         * @return true if successful, check errno if false
         */
        bool connect(const char *addr, uint16_t port);

        /**
         * @brief listens and accepts first incoming connection as a server, blocking
         *
         * @param port port to listen and accept connections on
         * @return true if successful, check errno if false
         */
        bool accept(uint16_t port);

        /**
         * @brief receives message
         *
         * @param[out] buf buffer to populate
         * @param size number of bytes to receive exactly
         * @param block true if should block
         * @return true if successful, check errno if false
         */
        bool recv(void *buf, size_t size, bool block = true);

        /**
         * @brief receives message when possible with timeout
         *
         * @param[out] buf buffer to populate
         * @param size number of bytes to receive exactly
         * @param timeout millisecond timeout to poll for (< 0 to block)
         * @return true if successful, check errno if false
         */
        bool recv_poll(void *buf, size_t size, int timeout);

        /**
         * @brief sends message
         *
         * @param buf buffer to send
         * @param size number of bytes to send exactly
         * @param block true if should block
         * @return true if successful, check errno if false
         */
        bool send(const void *buf, size_t size, bool block = true);

        /**
         * @brief sends message when possible with timeout
         *
         * @param buf buffer to send
         * @param size number of bytes to send exactly
         * @param timeout millisecond timeout to poll for (< 0 to block)
         * @return true if successful, check errno if false
         */
        bool send_poll(const void *buf, size_t size, int timeout);

        /**
         * @brief close connection
         *
         * @param shutdown force shutdown of connection
         */
        void close(bool shutdown = false);

    protected:
        /// @brief socket file descriptor, < 0 if not connected
        int _fd;

        /// @brief mutex for socket connect/close synchronization
        std::mutex _sck_mtx;
        /// @brief mutex for message recv/send synchronization
        std::mutex _msg_mtx;
    };

}