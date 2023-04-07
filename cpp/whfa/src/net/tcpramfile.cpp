/**
 * @file net/tcpramfile.cpp
 * @author Robert Griffith
 */
#include "net/tcpramfile.h"
#include "util/error.h"

#include <cstring>

namespace wu = whfa::util;

namespace
{

    /// @brief convenience alias for connection
    using WNTCPConnection = whfa::net::TCPConnection;

    /**
     * @brief close socket and free allocated memory
     *
     * @param[out] conn connection to close
     * @param[out] data byte buffer to free and set to nullptr
     */
    inline void close_ramfile(WNTCPConnection &conn, uint8_t *&data)
    {
        conn.close();
        if (data != nullptr)
        {
            delete data;
            data = nullptr;
        }
    }

    /**
     * @brief receive file size and initialize file data buffer
     *
     * @param[out] conn connection to use
     * @param[out] data byte buffer to create
     * @param[out] fsz file size in bytes
     * @param[out] rd_pos read position in file in bytes
     * @param[out] rv_pos receive position in file in bytes
     * @return true on success
     */
    inline bool init_ramfile(
        WNTCPConnection &conn,
        uint8_t *&data,
        uint64_t &fsz,
        uint64_t &rd_pos,
        uint64_t &rv_pos)
    {
        if (!conn.recv(&fsz, sizeof(fsz)))
        {
            close_ramfile(conn, data);
            return false;
        }
        data = new uint8_t[fsz];
        rd_pos = 0;
        rv_pos = 0;
        return true;
    }

}

namespace whfa::net
{

    /**
     * whfa::net::TCPRAMFile public methods
     */

    TCPRAMFile::TCPRAMFile(uint64_t blocksz)
        : _blocksz(blocksz),
          _data(nullptr),
          _filesz(0),
          _read_pos(0),
          _recv_pos(0)
    {
    }

    TCPRAMFile::~TCPRAMFile()
    {
        close();
    }

    bool TCPRAMFile::open(const char *addr, uint16_t port)
    {
        std::lock_guard<std::mutex> lk(_mtx);
        std::lock_guard<std::mutex> rd_lk(_read_mtx);
        close_ramfile(_conn, _data);
        bool success = false;
        if (!_conn.connect(addr, port))
        {
            set_state_stop(wu::ENET_CONNFAIL);
        }
        else if (!init_ramfile(_conn, _data, _filesz, _read_pos, _recv_pos))
        {
            set_state_stop(wu::ENET_TXFAIL);
        }
        else
        {
            success = true;
        }
        return success;
    }

    bool TCPRAMFile::open(uint16_t port)
    {
        std::lock_guard<std::mutex> lk(_mtx);
        std::lock_guard<std::mutex> rd_lk(_read_mtx);
        close_ramfile(_conn, _data);
        bool success = false;
        if (!_conn.accept(port))
        {
            set_state_stop(wu::ENET_CONNFAIL);
        }
        else if (!init_ramfile(_conn, _data, _filesz, _read_pos, _recv_pos))
        {
            set_state_stop(wu::ENET_TXFAIL);
        }
        else
        {
            success = true;
        }
        return success;
    }

    uint64_t TCPRAMFile::read(uint8_t *buf, uint64_t size)
    {
        std::lock_guard<std::mutex> rd_lk(_read_mtx);
        size = std::min(_filesz - _read_pos, size);
        if (_data == nullptr || size == 0)
        {
            return 0;
        }
        std::unique_lock<std::mutex> rv_lk(_recv_mtx);
        _recv_cond.wait(rv_lk, [=]
                        { return _recv_pos - _read_pos >= size; });
        rv_lk.unlock();
        memcpy(buf, _data, size);
        _read_pos += size;
        return size;
    }

    bool TCPRAMFile::seek(int64_t offset, int whence)
    {
        std::lock_guard<std::mutex> rd_lk(_read_mtx);
        if (_data == nullptr)
        {
            return false;
        }
        const bool neg = offset < 0;
        const int64_t signed_mag = neg ? -offset : offset;
        uint64_t mag;
        memcpy(&mag, &signed_mag, sizeof(mag));
        uint64_t base;
        switch (whence)
        {
        case SEEK_SET:
            base = 0;
            break;
        case SEEK_CUR:
            base = _read_pos;
            break;
        case SEEK_END:
            base = _filesz;
            break;
        default:
            return false;
        }
        const uint64_t pos = neg ? base - mag : base + mag;
        if ((neg && pos > base) || (!neg && pos < base))
        {
            return false;
        }
        std::unique_lock<std::mutex> rv_lk(_recv_mtx);
        _recv_cond.wait(rv_lk, [=]
                        { return _recv_pos >= pos; });
        rv_lk.unlock();
        _read_pos = pos;
        return true;
    }

    void TCPRAMFile::close()
    {
        std::lock_guard<std::mutex> rd_lk(_read_mtx);
        close_ramfile(_conn, _data);
        set_state_stop();
    }

    uint64_t TCPRAMFile::get_blocksize()
    {
        std::lock_guard<std::mutex> rd_lk(_read_mtx);
        return _blocksz;
    }

    uint64_t TCPRAMFile::get_filesize()
    {
        std::lock_guard<std::mutex> rd_lk(_read_mtx);
        return _filesz;
    }

    /**
     * whfa::net::TCPRAMFile protected methods
     */

    void TCPRAMFile::execute_loop_body()
    {
        const uint64_t sz = std::min(_blocksz, _filesz - _recv_pos);
        if (_data == nullptr || sz == 0)
        {
            set_state_stop();
        }
        else if (_conn.recv(&(_data[_recv_pos]), sz))
        {
            {
                std::lock_guard<std::mutex> rv_lk(_recv_mtx);
                _recv_pos += sz;
            }
            _recv_cond.notify_one();
        }
        else
        {
            set_state_pause(errno);
        }
    }
}