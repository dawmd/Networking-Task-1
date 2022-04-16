#ifndef __NETWORKING_H__
#define __NETWORKING_H__

#include <bit>
#include <cassert>
#include <cstdlib>
#include <cstring> // std::memcpy, std::strerror
#include <concepts>
#include <exception>
#include <string>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <endian.h>

#include <errno.h>

class InvalidType : public std::exception {
    virtual const char *what() const noexcept {
        return "The provided type is invalid.";
    }
};

class BufferOverflow : public std::exception {
    virtual const char *what() const noexcept {
        return "An attempt to overflow a buffer has been stopped.";
    }
};

class BindSocketError : public std::runtime_error {
public:
    BindSocketError(int errno_value)
    : std::runtime_error{
        std::string{"Binding a socket has failed with errno set to code "}
        + std::strerror(errno) + "."
    } {}
};

class ReceiveError : public std::runtime_error {
public:
    ReceiveError(int errno_value)
    : std::runtime_error{
        std::string{"An attempt to receive a message has failed with errno set to code"}
        + std::strerror(errno) + "."
    } {}
};

class SendError : public std::exception {
    virtual const char *what() const noexcept {
        return "A message has not been sent completely.";
    }
};

class NetworkReader {
private:
    const char const   *m_buffer;
    std::size_t         m_offset;
    const std::size_t   m_buffer_size;

public:
    NetworkReader() = delete;
    NetworkReader(char const *buffer, std::size_t size)
    : m_buffer{buffer}
    , m_offset(0)
    , m_buffer_size{size}
    {
        assert(m_buffer);
    }
    ~NetworkReader() = default;

    template<typename T>
        requires std::integral<T> || std::floating_point<T>
    T read_number() {
        switch (sizeof(T)) {
            case 1:
            case 2:
            case 4:
            case 8:     break;
            default:    throw InvalidType(); // should never occur
        }

        char bytes[8];
        get_bytes(bytes, sizeof(T));
        
        switch (sizeof(T)) {
            case 1:
                return std::bit_cast<T>(*bytes);
            case 2:
                return std::bit_cast<T>(htobe16(std::bit_cast<uint16_t>(*bytes)));
            case 4:
                return std::bit_cast<T>(htobe32(std::bit_cast<uint32_t>(*bytes)));
            case 8:
                return std::bit_cast<T>(htobe64(std::bit_cast<uint64_t>(*bytes)));
            default:
                // never occurs
                assert(false);
        }
    }

    void read_bytes(char *bytes, std::size_t length) {
        if (m_buffer_size - m_offset < length)
            throw BufferOverflow();
        std::memcpy(bytes, &m_buffer[m_offset], length);
        m_offset += length;
    }

    void read_bytes(std::string &bytes, std::size_t length) {
        if (m_buffer_size - m_offset < length)
            throw BufferOverflow();
        bytes.append(&m_buffer[m_offset], length);
        m_offset += length;
    }

    void read_bytes(std::string &bytes) {
        read_bytes(bytes, m_buffer_size - m_offset);
    }

    std::size_t size() const noexcept {
        return m_buffer_size;
    }

    std::size_t read_length() const noexcept {
        return m_offset;
    }
};

class NetworkWriter {
private:
    char               *m_buffer;
    std::size_t         m_offset;
    const std::size_t   m_buffer_size;

public:
    NetworkWriter() = delete;
    NetworkWriter(std::size_t buffer_size)
    : m_buffer_size{buffer_size}
    , m_offset(0)
    {
        m_buffer = new char[m_buffer_size];
    }
    ~NetworkWriter() {
        if (m_buffer)
            delete [] m_buffer;
    }
    
    template<typename T>
        requires std::integral<T> || std::floating_point<T>
    void add_number(T number) {
        switch (sizeof(T)) {
            case 1:
                break;
            case 2:
                number = std::bit_cast<T>(htobe16(std::bit_cast<uint16_t>(number)));
                break;
            case 4:
                number = std::bit_cast<T>(htobe32(std::bit_cast<uint32_t>(number)));
                break;
            case 8:
                number = std::bit_cast<T>(htobe64(std::bit_cast<uint64_t>(number)));
                break;
            default:
                // In practice, this case never occurs
                throw InvalidType();
        }
        char *bytes = std::bit_cast<char*>(&number);
        write_to_buffer(bytes, sizeof(T));
    }

    void write_to_buffer(char const *bytes, std::size_t length) {
        assert(bytes);
        if (m_buffer_size - m_offset < length)
            throw BufferOverflow();
        std::memcpy(&m_buffer[m_offset], bytes, length);
        m_offset += length;
    }

    void write_to_buffer(const std::string &bytes, std::size_t length) {
        assert(bytes.length() >= length);
        if (m_buffer_size - m_offset < length)
            throw BufferOverflow();
        bytes.copy(&m_buffer[m_offset], length);
        m_offset += bytes.length();
    }

    void write_to_buffer(const std::string &bytes) {
        write_to_buffer(bytes, bytes.length());
    }

    std::size_t length() const noexcept {
        return m_offset;
    }

    std::size_t size() const noexcept {
        return m_buffer_size;
    }
};

// IP_V4, UDP
int bind_socket(uint16_t port) {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htobe32(INADDR_ANY);
    server_address.sin_port = htobe16(port);

    if (bind(socket_fd, (sockaddr*) &server_address,
             static_cast<socklen_t>(sizeof(server_address))))
    {
        throw BindSocketError(errno);
    }

    return socket_fd;
}

std::size_t read_message(int socket_fd, sockaddr_in &client_address,
                         char *buffer, std::size_t max_length)
{
    socklen_t address_length = static_cast<socklen_t>(sizeof(client_address));
    int flags = 0;
    ssize_t len = recvfrom(socket_fd, buffer, max_length, flags,
                           reinterpret_cast<sockaddr*>(&client_address), &address_length);
    if (len == -1)
        throw ReceiveError(errno);
    return static_cast<std::size_t>(len);
}

void send_message(int socket_fd, const sockaddr_in &client_address,
                  const char *message, std::size_t length)
{
    socklen_t address_length = static_cast<socklen_t>(sizeof(client_address));
    int flags = 0;
    ssize_t sent_length = sendto(socket_fd, message, length, flags,
                                 reinterpret_cast<const sockaddr*>(&client_address),
                                 address_length);
    if (sent_length != static_cast<ssize_t>(length))
        throw SendError();
}

#endif // __NETWORKING_H__

