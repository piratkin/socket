#pragma once
#include <cstdint> /* uintX_t */
#include <climits> /* INT_MAX */
#include <vector> /* vector */
#include <cstddef> /* size_t */
#include <algorithm> /* copy min */
#include <iterator> /* back_inserter */
#include <system_error> /* system_error */
#include <fcntl.h> /* fcntl */
#include <variant> /* variant */
#include <utility> /* pair make_pair */
#include <optional> /* optional nullopt */
#include <cerrno> /* errno */
#include <cstring> /* memset strerror_r */
#include <span> /* span */
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime> /* time_t */

#ifdef __linux__
#include <sys/socket.h> /* socket setsockopt recv */
#include <unistd.h> /* close */
#include <netinet/in.h> /* htons */
#include <sys/select.h> /* select FD_ZERO FD_SET */
#include <netinet/tcp.h> /* TCP_NODELAY */
#include <arpa/inet.h> /* inet_aton */
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR   (-1)
#define closesocket close
#else
#pragma warning(disable:4005)
#include <winsock2.h> /* WSAStartup WSACleanup WSAGetLastError */
#pragma warning(default:4005) 
#include <WS2tcpip.h>
//#define close closesocket
#define inet_aton(...) inet_pton(AF_INET, ##__VA_ARGS__)
#pragma comment(lib, "Ws2_32.lib")
#endif /* __linux__*/

constexpr size_t DEFAULT_TX_TIMEOUT = 1000UL;
constexpr size_t DEFAULT_RX_TIMEOUT = 30000UL;
constexpr size_t MAX_SEND_BUFFER_SIZE = 1000000UL;
constexpr size_t MAX_READ_BUFFER_SIZE = 1024UL;

class Socket {

    SOCKET fd = INVALID_SOCKET;
#ifndef __linux__
    WSADATA wsad = {0};
#endif /* __linux__ */

    static void _makesock(sockaddr_in &sock,
        uint16_t port, const std::string& address) {
        memset(&sock, 0, sizeof(sock));
#ifdef HAVE_SOCK_SIN_LEN
        sock.sin_len = sizeof(sock);
#endif
        sock.sin_port = htons(port);
        sock.sin_family = AF_INET;
        if (address.empty()) {
            sock.sin_addr.s_addr = INADDR_ANY;
            return;
        }
        if (inet_aton(address.c_str(), &sock.sin_addr) == 0) {
            throw std::runtime_error("Invalid inet address!");
        }
    }

    static std::variant<std::chrono::time_point<
        std::chrono::system_clock>, int64_t> _timeout(
            std::variant<std::chrono::time_point<
                std::chrono::system_clock>, int64_t> 
                    *pvtp = nullptr) {
        auto last = std::chrono::system_clock::now();
        if (pvtp == nullptr) return last;
        std::chrono::time_point tp = 
            std::get<std::chrono::time_point<
                std::chrono::system_clock>>(*pvtp);
        return std::chrono::duration_cast<
            std::chrono::milliseconds>(last - tp).count();
    }

    static bool _retry(std::optional<const int> error = std::nullopt) {
        auto e = error.value_or(_error());
        switch (e) {
            case EAGAIN: /* EWOULDBLOCK */
#ifdef __linux__
            case EINPROGRESS:
            case EINTR:
#else
            case WSAEINPROGRESS:
            case WSAEINTR:
            case WSAEWOULDBLOCK: /* 10035 */
#endif /* __linux__ */
                return true;
        }
        constexpr size_t ERROR_MESSAGE_SIZE = 255;
        char buffer[ERROR_MESSAGE_SIZE] = {0};
#ifdef __linux__
        strerror_r(e, buffer, ERROR_MESSAGE_SIZE - 1);
#else
        strerror_s(buffer, ERROR_MESSAGE_SIZE - 1, e);
#endif /* __linux__ */
        std::cerr << "Error: " << buffer << std::endl;
        return false;
    }

    static int _error() {
#ifdef __linux__
        int _errno = errno;
#else
        int _errno = WSAGetLastError();
#endif /* __linux__ */
        return _errno;
    }

    void _init(const int flag) {
#ifdef _MSC_VER
        if (WSAStartup(MAKEWORD(2, 2), &wsad) != 0) {
            throw std::system_error(errno, std::system_category(),
                "Error: call WSAStartup!");
        }
#endif /* _MSC_VER */
        fd = socket(AF_INET, flag, 0);
        if (fd == INVALID_SOCKET) {
            throw std::system_error(errno, std::system_category(),
                "Error: create socket!");
        }
#if defined(FD_CLOEXEC)
        if (!fcntl(FD_CLOEXEC, true)) {
            throw std::system_error(errno, std::system_category(), 
                "Error: fcntl FD_CLOEXEC!");
        }
#endif
#if defined(O_NONBLOCK)
        if (!fcntl(O_NONBLOCK, false)) {
            throw std::system_error(errno, std::system_category(),
                "Error: fcntl O_NONBLOCK!");
        }
#endif
#if defined(FIONBIO) and defined(_MSC_VER)
        if (!ioctlsock(FIONBIO, true)) {
            throw std::system_error(errno, std::system_category(),
                "Error: fcntl FIONBIO!");
        }
#endif
        if (!sockopt(SOL_SOCKET, SO_REUSEADDR)) {
            throw std::system_error(errno, std::system_category(),
                "Error: sockopt SOL_SOCKET SO_REUSEADDR!");
        }
        if (!sockopt(SOL_SOCKET, SO_KEEPALIVE)) {
            throw std::system_error(errno, std::system_category(),
                "Error: sockopt SOL_SOCKET SO_KEEPALIVE!");
        }
        if (flag == SOCK_DGRAM) return;
        if (!sockopt(IPPROTO_TCP, TCP_NODELAY)) {
            throw std::system_error(errno, std::system_category(),
                "Error: sockopt IPPROTO_TCP TCP_NODELAY!");
        }
    }

    void _dispose() {
#ifdef _MSC_VER
        if (WSACleanup() != 0) {
            throw std::system_error(errno, std::system_category(),
                "Error call WSAStartup!");
        }
#endif /* _MSC_VER */
    }

public:
    explicit Socket(bool udp = false) noexcept {
        try {
            _init(udp ? SOCK_DGRAM : SOCK_STREAM);
        } catch (const std::exception &e) {
            std::cerr << e.what() << std::endl;
        }
    }

    explicit Socket(SOCKET sock) : fd(sock) {}

    ~Socket() noexcept {
        try {
            _dispose();
        } catch (const std::exception &e) {
            std::cerr << e.what() << std::endl;
        }
        if (fd == INVALID_SOCKET) return;
        ::closesocket(fd);
        fd = INVALID_SOCKET;
    }

    [[nodiscard]] bool isOk() const {
        return fd != INVALID_SOCKET;
    }

    [[nodiscard]] bool pollin(
        const long timeout = DEFAULT_RX_TIMEOUT) const {
        fd_set rfds = {0};
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        struct timeval tv {
            .tv_sec = timeout / 1000L,
            .tv_usec = 1000L * (timeout % 1000L)
        };
        int rc = ::select((int) fd + 1, &rfds, nullptr, nullptr, &tv);
        return (rc > 0);
    }

    [[nodiscard]] bool pollout(
        const long timeout = DEFAULT_TX_TIMEOUT) const {
        fd_set wfds = {0};
        FD_ZERO(&wfds);
        FD_SET(fd, &wfds);
        struct timeval tv {
            .tv_sec = timeout / 1000L,
            .tv_usec = 1000L * (timeout % 1000L)
        };
        int rc = ::select((int) fd + 1, nullptr, 
            &wfds, nullptr, &tv);
        return (rc > 0);
    }

    [[nodiscard]] bool sockopt(const int level, const int flag, 
        std::optional<std::pair< const char*, int>> 
            opt = std::nullopt) const noexcept {
        constexpr static int defopt = 1;
        if (!opt.has_value()) {
            auto ptr = reinterpret_cast<const char*>(&defopt);
            int size = sizeof(defopt);
            opt = std::make_pair(ptr, size);
        }
        return ::setsockopt(fd, level, flag, 
            opt->first, opt->second) == 0;
    }

#ifdef __linux__
    [[nodiscard]] bool fcntl(const int flag,
        const bool state = true) const noexcept {
        int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags == SOCKET_ERROR) return false;
        flags = (state) ? (flags | flag) : (flags & ~flag);
        int rc = ::fcntl(fd, F_SETFL, flags);
        return (rc != SOCKET_ERROR);
    }
#else

    bool ioctlsock(const int flag, const bool state = true) noexcept {
        unsigned long _state = state ? 1 : 0;
        return (ioctlsocket(fd, flag, &_state) == NO_ERROR);
    }

#endif /* __linux__ */

    [[nodiscard]] bool connect(const uint16_t port, 
        const std::string& address, 
        const uint32_t timeout = DEFAULT_RX_TIMEOUT) const {
        sockaddr_in sock{};
        _makesock(sock, port, address);
        int rc = ::connect(fd, (sockaddr*)&sock, sizeof(sock));
        if (rc == SOCKET_ERROR && _retry()) return pollout(timeout);
        return (rc != SOCKET_ERROR);
    }

    [[nodiscard]] bool connect(const uint16_t port,
        const std::string& address, const uint16_t sourcePort,
        const std::string& sourceAddress,
        const uint32_t timeout = DEFAULT_RX_TIMEOUT) const {
        sockaddr_in source{};
        _makesock(source, sourcePort, sourceAddress);
        int rc = ::bind(fd, (sockaddr*)&source, sizeof(source));
        if (rc == SOCKET_ERROR) return false;
        sockaddr_in target{};
        _makesock(target, port, address);
        rc = ::connect(fd, (sockaddr*)&target, sizeof(target));
        if (rc == SOCKET_ERROR && _retry()) return pollout(timeout);
        return (rc != SOCKET_ERROR);
    }

    [[nodiscard]] bool bind(const uint16_t port,
        const std::optional<const std::string>& 
            address = std::nullopt) const {
        sockaddr_in sock{};
        _makesock(sock, port, address.value_or(""));
        int rc = ::bind(fd, (sockaddr*)&sock, sizeof(sock));
        if (rc == SOCKET_ERROR) return false;
        ::listen(fd, SOMAXCONN);
        return true;
    }

    Socket ready(uint16_t *port = nullptr,
        std::string *address = nullptr) const {
        sockaddr_in sock{};
        socklen_t size = sizeof(sock);
        SOCKET _fd = ::accept(fd, (struct sockaddr*)&sock, &size);
        if (address != nullptr) *address = inet_ntoa(sock.sin_addr);
        if (port != nullptr) *port = ntohs(sock.sin_port);
        return Socket(_fd);
    }

    [[nodiscard]] bool send(const std::vector<uint8_t> &data,
        const uint32_t timeout = DEFAULT_TX_TIMEOUT) const {
        if (data.empty()) return false;
        size_t size = data.size();
        auto _size = (std::min)(MAX_SEND_BUFFER_SIZE, size);
        if (_size > INT_MAX) return false;
        int sz = static_cast<int>(_size);
        auto ptr = data.data();
        auto start = _timeout();
        while (size) {
            if (auto diff = _timeout(&start);
                std::get<int64_t>(diff) > timeout) {
                break;
            }
            int rc = 0;
            if (pollout()) {
                rc = ::send(fd, (const char *) ptr, sz, 0);
            } else if (data.data() == ptr) {
                continue;
            } else {
                break;
            }
            if (rc > 0) {
                ptr += rc;
                size -= rc;
            } else if (rc == 0 || !_retry()) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] std::vector<uint8_t> read(size_t size = 0,
        const uint32_t timeout = DEFAULT_RX_TIMEOUT) const {
        std::vector <uint8_t> data;
        uint8_t buffer[MAX_READ_BUFFER_SIZE] = {0};
        auto start = _timeout();
        for (bool loop = true; loop; loop = true) {
            if (auto diff = _timeout(&start);
                std::get<int64_t>(diff) > timeout) {
                break;
            }
            int rc = 0;
            if (pollin()) {
                rc = ::recv(fd, (char*)buffer, MAX_READ_BUFFER_SIZE, 0);
                // rc = ::read(fd, buffer, MAX_READ_BUFFER_SIZE);
            } else if (data.empty()) {
                continue;
            } else {
                break;
            }
            if (rc > 0) {
                std::copy(buffer, buffer + rc, std::back_inserter(data));
                size -= (size < rc) ? size : rc;
                loop = size;
                continue;
            } else if (rc < 0 && _retry() && pollin(timeout)) {
                continue;
            } else {
                break;
            }
        }
        return data;
    }
};
