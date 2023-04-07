/**
 * @file net/tcpconnection.cpp
 * @author Robert Griffith
 */
#include "net/tcpconnection.h"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{

    /**
     * @brief close file descriptor and clear value (-1), optional forced shutdown
     *
     * @param[out] fd file descriptor of socket
     * @param sd force shutdown of socket
     */
    inline void close_conn(int &fd, bool sd = false)
    {
        if (fd < 0)
        {
            return;
        }
        if (sd)
        {
            shutdown(fd, SHUT_RDWR);
        }
        close(fd);
        fd = -1;
    }

    /**
     * @brief loop body for full recv with non-zero size
     *
     * @param fd file descriptor to recv from
     * @param[out] data byte buffer to populate and update position
     * @param[out] size number of bytes (must be > 0) to recv
     * @param flags recv flags
     * @return true if successful, check errno if false
     */
    bool recv_loop_body(int fd, uint8_t *&data, size_t &size, int flags)
    {
        bool success = false;
        const ssize_t rv = ::recv(fd, data, size, flags);
        // rv == 0 for EOF is error, since size > 0
        if (rv > 0)
        {
            data += rv;
            size -= static_cast<size_t>(rv);
            success = true;
        }
        else if (errno == EINTR)
        {
            // continue attempts upon interruption
            success = true;
        }
        return success;
    }

    /**
     * @brief loop body for full send
     *
     * @param fd file descriptor to send to
     * @param[out] data byte buffer to populate and update position
     * @param[out] size number of bytes to send
     * @param flags send flags
     * @return true if successful, check errno if false
     */
    bool send_loop_body(int fd, const uint8_t *&data, size_t &size, int flags)
    {
        bool success = false;
        const ssize_t rv = ::send(fd, data, size, flags);
        if (rv >= 0)
        {
            data += rv;
            size -= static_cast<size_t>(rv);
            success = true;
        }
        else if (errno == EINTR)
        {
            // continue attempts upon interruption
            success = true;
        }
        return success;
    }

}

namespace whfa::net
{

    /**
     * whfa::net::TCPConnection public methods
     */

    TCPConnection::TCPConnection(int fd)
        : _fd(fd)
    {
    }

    TCPConnection::~TCPConnection()
    {
        close();
    }

    bool TCPConnection::connect(const char *addr, uint16_t port)
    {
        std::lock_guard<std::mutex> s_lk(_sck_mtx);
        close_conn(_fd);
        struct sockaddr_in sa;
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        if (inet_pton(AF_INET, addr, &sa.sin_addr) <= 0)
        {
            return false;
        }
        if ((_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            return false;
        }
        if (::connect(_fd, reinterpret_cast<sockaddr *>(&sa), sizeof(sa)) < 0)
        {
            close_conn(_fd);
            return false;
        }
        return true;
    }

    bool TCPConnection::accept(uint16_t port)
    {
        std::lock_guard<std::mutex> s_lk(_sck_mtx);
        close_conn(_fd);
        int sfd;
        if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            return false;
        }
        struct sockaddr_in sa;
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        sa.sin_addr.s_addr = INADDR_ANY;
        const int enable = 1;
        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                       &enable, sizeof(enable)) < 0)
        {
            close_conn(sfd);
            return false;
        }
        if (bind(sfd, reinterpret_cast<sockaddr *>(&sa), sizeof(sa)) < 0)
        {
            close_conn(sfd);
            return false;
        }
        if (listen(sfd, 1) < 0)
        {
            close_conn(sfd);
            return false;
        }
        socklen_t sasz = sizeof(sa);
        _fd = ::accept(sfd, reinterpret_cast<sockaddr *>(&sa), &sasz);
        close_conn(sfd);
        return _fd >= 0;
    }

    bool TCPConnection::recv(void *buf, size_t size, bool block)
    {
        std::lock_guard<std::mutex> m_lk(_msg_mtx);
        const int flags = block ? 0 : MSG_DONTWAIT;
        uint8_t *data = static_cast<uint8_t *>(buf);
        while (size > 0)
        {
            if (!recv_loop_body(_fd, data, size, flags))
            {
                return false;
            }
        }
        return true;
    }

    bool TCPConnection::recv_poll(void *buf, size_t size, int timeout)
    {
        std::lock_guard<std::mutex> m_lk(_msg_mtx);
        uint8_t *data = static_cast<uint8_t *>(buf);
        struct pollfd pfd
        {
            .fd = _fd,
            .events = POLLIN
        };
        while (size > 0)
        {
            const int rv = poll(&pfd, 1, timeout);
            if (rv > 0 && pfd.revents & POLLIN)
            {
                if (!recv_loop_body(_fd, data, size, MSG_DONTWAIT) &&
                    (errno != EAGAIN) && (errno != EWOULDBLOCK))
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
        return true;
    }

    bool TCPConnection::send(const void *buf, size_t size, bool block)
    {
        std::lock_guard<std::mutex> m_lk(_msg_mtx);
        const int flags = block ? 0 : MSG_DONTWAIT;
        const uint8_t *data = static_cast<const uint8_t *>(buf);
        while (size > 0)
        {
            if (!send_loop_body(_fd, data, size, flags))
            {
                return false;
            }
        }
        return true;
    }

    bool TCPConnection::send_poll(const void *buf, size_t size, int timeout)
    {
        std::lock_guard<std::mutex> m_lk(_msg_mtx);
        const uint8_t *data = static_cast<const uint8_t *>(buf);
        struct pollfd pfd
        {
            .fd = _fd,
            .events = POLLOUT
        };
        while (size > 0)
        {
            const int rv = poll(&pfd, 1, timeout);
            if (rv > 0 && pfd.revents & POLLIN)
            {
                if (!send_loop_body(_fd, data, size, MSG_DONTWAIT) &&
                    (errno != EAGAIN) && (errno != EWOULDBLOCK))
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
        return true;
    }

    void whfa::net::TCPConnection::close(bool shutdown)
    {
        std::lock_guard<std::mutex> s_lk(_sck_mtx);
        close_conn(_fd, shutdown);
    }
}