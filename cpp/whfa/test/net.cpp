/**
 * @file test/pcm.cpp
 * @author Robert Griffith
 */
#include "test/net.h"

#include "net/tcpconnection.h"
#include "net/tcpramfile.h"
#include "util/error.h"

#include <cstring>
#include <iostream>
#include <random>
#include <thread>

namespace wn = whfa::net;
namespace wu = whfa::util;

namespace
{

    /// @brief prefix string for logging from client
    constexpr const char *__PRFXCLNT = "CLIENT: ";
    /// @brief prefix string for logging from server
    constexpr const char *__PRFXSRVR = "SERVER: ";
    /// @brief size of buffer in bytes used to test transfers
    constexpr size_t __BUFSZ = 8192;
    /// @brief number of buffers to send from each side
    constexpr size_t __NBUFSENDS = 8;
    /// @brief number of buffers to send from each side
    constexpr size_t __NBUFS = 2 * __NBUFSENDS;

    /// @brief random number generator
    std::default_random_engine __gen;
    /// @brief local mutex to synchronize generator usage
    std::mutex __gen_mtx;

    /// @brief local tcp ram file
    wn::TCPRAMFile __f;
    /// @brief local mutex to synchronize local object usage
    std::mutex __mtx;

    /**
     * @brief initialize random byte buffers
     *
     * @return random byte buffers
     */
    uint8_t *init_bufs()
    {
        std::lock_guard<std::mutex> lk(__gen_mtx);
        std::uniform_int_distribution<size_t> dist;
        uint8_t *out = new uint8_t[__NBUFS * __BUFSZ];
        for (size_t i = 0; i < __NBUFS * __BUFSZ - sizeof(size_t); ++i)
        {
            const size_t r = dist(__gen);
            memcpy(&(out[i]), &r, sizeof(size_t));
        }
        return out;
    }

    /**
     * @brief copy byte buffers
     *
     * @param bufs
     */
    uint8_t *copy_bufs(const uint8_t *bufs)
    {
        uint8_t *out = new uint8_t[__NBUFS * __BUFSZ];
        memcpy(out, bufs, __NBUFS * __BUFSZ);
        return out;
    }

    /**
     * @brief compare buffers
     *
     * @param bufs_a buffers that should match bufs_b
     * @param bufs_b buffers that should match bufs_a
     */
    void compare_bufs(const uint8_t *bufs_a, const uint8_t *bufs_b)
    {
        for (size_t i = 0; i < __NBUFS * __BUFSZ; i += __BUFSZ)
        {
            if (memcmp(&(bufs_a[i]), &(bufs_b[i]), __BUFSZ) != 0)
            {
                std::cerr << "ERROR: buffers at byte " << i << " differ" << std::endl;
            }
        }
    }

    /**
     * @brief client testing of simple connection
     *
     * @param port local port to test connection with self
     * @param[out] bufs buffers to send and receive
     */
    void test_tcpconnection_client(uint16_t port, uint8_t *bufs)
    {
        wn::TCPConnection c;
        c.connect("127.0.0.1", port);
        for (size_t i = 0; i < __NBUFS * __BUFSZ; i += 2 * __BUFSZ)
        {
            if (!c.send(&(bufs[i]), __BUFSZ))
            {
                std::cerr << __PRFXCLNT << "failed to send buffer " << i << std::endl;
                break;
            }
            if (!c.recv(&(bufs[i + __BUFSZ]), __BUFSZ))
            {
                std::cerr << __PRFXCLNT << "failed to receive buffer " << i + 1 << std::endl;
                break;
            }
        }
        c.close();
    }

    /**
     * @brief server testing of simple connection
     *
     * @param port local port to test connection with self
     * @param[out] bufs buffers to send and receive
     */
    void test_tcpconnection_server(uint16_t port, uint8_t *bufs)
    {
        wn::TCPConnection c;
        c.accept(port);
        for (size_t i = 0; i < __NBUFS * __BUFSZ; i += 2 * __BUFSZ)
        {
            if (!c.send(&(bufs[i + __BUFSZ]), __BUFSZ))
            {
                std::cerr << __PRFXSRVR << "failed to send buffer " << i + 1 << std::endl;
                break;
            }
            if (!c.recv(&(bufs[i]), __BUFSZ))
            {
                std::cerr << __PRFXSRVR << "failed to receive buffer " << i << std::endl;
                break;
            }
        }
        c.close();
    }

    /**
     * @brief client testing of sending file
     *
     * @param port local port to test connection with self
     * @param[out] bufs buffers to send as part of file
     */
    void test_tcpramfile_client(uint16_t port, uint8_t *bufs)
    {
        wn::TCPConnection c;
        if (!c.connect("127.0.0.1", port))
        {
            std::cerr << __PRFXCLNT << "failed to connect as client" << std::endl;
            return;
        }
        const uint64_t fsz = static_cast<uint64_t>(__NBUFS * __BUFSZ);
        if (!c.send(&fsz, sizeof(fsz)))
        {
            std::cerr << __PRFXCLNT << "failed to send filesize" << std::endl;
        }
        else if (!c.send(bufs, __NBUFS * __BUFSZ))
        {
            std::cerr << __PRFXCLNT << "failed to send file" << std::endl;
        }
        c.close();
    }

    /**
     * @brief server testing of receiving file into ram
     *
     * @param port local port to test connection with self
     * @param[out] bufs buffers to read file into
     */
    void test_tcpramfile_server_noseek(uint16_t port, uint8_t *bufs)
    {
        std::unique_lock<std::mutex> lk(__mtx);
        if (!__f.open(port))
        {
            std::cerr << __PRFXSRVR << "failed to open ramfile" << std::endl;
            wu::Threader::State s;
            __f.get_state(s);
            wu::print_error(s.error);
        }
        else
        {
            __f.start();
            size_t i = 0;
            size_t rv;
            while ((rv = __f.read(&(bufs[i]), __BUFSZ)) != 0)
            {
                if (rv != __BUFSZ)
                {
                    std::cerr << __PRFXSRVR << "expected to read " << __BUFSZ << " bytes at pos " << i << " but received " << rv << std::endl;
                }
                i += rv;
            }
        }
        __f.close();
    }

    /**
     * @brief server testing of receiving file into ram
     *
     * @param port local port to test connection with self
     * @param[out] bufs buffers to read file into
     */
    void test_tcpramfile_server_seek(uint16_t port, uint8_t *bufs)
    {
        std::unique_lock<std::mutex> lk(__mtx);
        if (!__f.open(port))
        {
            std::cerr << __PRFXSRVR << "failed to open ramfile" << std::endl;
            wu::Threader::State s;
            __f.get_state(s);
            wu::print_error(s.error);
        }
        else
        {
            __f.start();
            size_t rv;
            // alternating seeking reads for last half of file
            size_t i = __NBUFSENDS * __BUFSZ;
            if (!__f.seek(i, SEEK_CUR))
            {
                std::cerr << __PRFXSRVR << "failed initial seek to middle of file" << std::endl;
                return;
            }
            while (i < __NBUFS * __BUFSZ)
            {
                if (!__f.seek(i + __BUFSZ, SEEK_CUR))
                {
                    std::cerr << __PRFXSRVR << "failed seek to byte " << i + __BUFSZ << std::endl;
                    return;
                }
                if ((rv = __f.read(&(bufs[i + __BUFSZ]), __BUFSZ)) != __BUFSZ)
                {
                    std::cerr << __PRFXSRVR << "failed to read into byte " << i + __BUFSZ << std::endl;
                    std::cerr << __PRFXSRVR << "only read " << rv << " of " << __BUFSZ << " bytes" << std::endl;
                    return;
                }
                if (!__f.seek(-2 * __BUFSZ, SEEK_CUR))
                {
                    std::cerr << __PRFXSRVR << "failed seek to byte " << i << std::endl;
                    return;
                }
                if ((rv = __f.read(&(bufs[i]), __BUFSZ)) != __BUFSZ)
                {
                    std::cerr << __PRFXSRVR << "failed to read into byte " << i << std::endl;
                    std::cerr << __PRFXSRVR << "only read " << rv << " of " << __BUFSZ << " bytes" << std::endl;
                    return;
                }
                i += 2 * __BUFSZ;
            }
            // straight through reads for first half of file
            i = 0;
            if (!__f.seek(i, SEEK_SET))
            {
                std::cerr << __PRFXSRVR << "failed midway seek to beginning of file" << std::endl;
                return;
            }
            while (i < __NBUFSENDS * __BUFSZ)
            {
                rv = __f.read(&(bufs[i]), __BUFSZ);
                if (rv != __BUFSZ)
                {
                    std::cerr << __PRFXSRVR << "expected to read " << __BUFSZ << " bytes at pos " << i << " but received " << rv << std::endl;
                }
                i += rv;
            }
        }
        __f.close();
    }

}

namespace whfa::test
{

    void test_tcpconnection(uint16_t port)
    {
        std::cout << "TESTING " << __func__ << std::endl;
        uint8_t *bufs_c = init_bufs();
        uint8_t *bufs_s = copy_bufs(bufs_c);
        std::thread t_c(test_tcpconnection_client, port, bufs_c);
        test_tcpconnection_server(port, bufs_s);
        t_c.join();
        compare_bufs(bufs_c, bufs_s);
        // cleanup
        delete bufs_c;
        delete bufs_s;
        std::cout << "DONE with " << __func__ << std::endl;
    }

    void test_tcpramfile(uint16_t port)
    {
        std::cout << "TESTING " << __func__ << std::endl;
        uint8_t *bufs_c = init_bufs();
        uint8_t *bufs_s = copy_bufs(bufs_c);
        // no seeking, direct until read returns 0
        std::cout << "testing with no seeking" << std::endl;
        memset(bufs_s, 0, __NBUFS * __BUFSZ);
        std::thread t_c_n(test_tcpramfile_client, port, bufs_c);
        test_tcpramfile_server_noseek(port, bufs_s);
        t_c_n.join();
        compare_bufs(bufs_c, bufs_s);
        // seeking allowed, recopy in case of discrepancy
        std::cout << "testing with seeking" << std::endl;
        memset(bufs_s, 0, __NBUFS * __BUFSZ);
        std::thread t_c_s(test_tcpramfile_client, port, bufs_c);
        test_tcpramfile_server_seek(port, bufs_s);
        t_c_s.join();
        compare_bufs(bufs_c, bufs_s);
        // cleanup
        delete bufs_c;
        delete bufs_s;
        std::cout << "DONE with " << __func__ << std::endl;
    }

}