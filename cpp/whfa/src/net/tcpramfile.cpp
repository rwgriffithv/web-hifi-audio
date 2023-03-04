/**
 * @file tcpramfile.cpp
 * @author Robert Griffith
 */
#include "net/tcpramfile.h"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{

    /**
     * @brief close socket and free allocated memory
     *
     * @param[out] fd file descriptor to close, -1 = not open
     * @param[out] data byte buffer to free and set to nullptr
     * @return 0 on success, error value otherwise
     */
    inline int close_ramfile(int &fd, uint8_t *&data)
    {
        int rv = 0;
        if (fd >= 0)
        {
            rv = close(fd);
            fd = -1;
        }
        if (data != nullptr)
        {
            delete data;
            data = nullptr;
        }
        return rv;
    }

    bool recv_all(int fd, uint8_t *data, size_t &size, int timeout_ms)
    {
        int rv;
        struct pollfd pfd
        {
            .fd = fd,
            .events = 0
        };
        while (size > 0)
        {
            // polling with timeout so fd can be closed
            // TODO: poll and everything should aquire filemtx and release before next poll
            // TODO: this gives fd a chance to be closed

            // TODO: loop body can be put into execute_loop_body
            // TODO: won't need extra mutex, dont' need to read exactly "size" bytes, just recv and write to buffer
            rv = poll(&pfd, 1, timeout_ms);
            if (rv > 0)
            {
                int res = recv(fd, data, size, MSG_DONTWAIT);
                if (res > 0)
                {
                    data += res;
                    size -= res;
                }
                else if (res == 0)
                {
                    // EOF
                    return true;
                }
                else if ((errno != EAGAIN) && (errno != EWOULDBLOCK) && (errno != EINTR))
                {
                    return false;
                }
            }
            else if (rv < 0)
            {
                return false;
            }
        }
        return true;
    }

}

namespace whfa::net
{

    /**
     * whfa::net::TCPRAMFile public methods
     */

    TCPRAMFile::TCPRAMFile(size_t blocksz)
        : _fd(-1),
          _blocksz(blocksz),
          _data(nullptr)
    {
    }

    TCPRAMFile::~TCPRAMFile()
    {
        close();
    }

    bool TCPRAMFile::open(const char *addr, uint16_t port)
    {
        std::lock_guard<std::mutex> lk(_mtx);
        std::lock_guard<std::mutex> f_lk(_filemtx);
        close_ramfile(_fd, _data);

        struct sockaddr_in sa;
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        if (inet_pton(AF_INET, addr, &sa.sin_addr) <= 0)
        {
            set_state_stop(errno);
            return false;
        }

        if ((_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            set_state_stop(errno);
            return false;
        }
        if (connect(_fd, reinterpret_cast<sockaddr *>(&sa), sizeof(sa)) < 0)
        {
            set_state_stop(errno);
            close_ramfile(_fd, _data);
            return false;
        }
        /// @todo: receive file size (required for seeking)
        /// @todo: send block size
        return true;
    }

    int TCPRAMFile::read(uint8_t *buf, size_t size)
    {
        /// @todo
        return 0;
    }
    bool TCPRAMFile::seek(int64_t offset, int whence)
    {
        /// @todo
        return false;
    }
    void TCPRAMFile::close()
    {
        std::lock_guard<std::mutex> f_lk(_filemtx);
        const int rv = close_ramfile(_fd, _data);
        set_state_stop(rv);
    }

    /**
     * whfa::net::TCPRAMFile protected methods
     */

    void TCPRAMFile::execute_loop_body()
    {
        /// @todo
    }
}