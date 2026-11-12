#pragma once

#include <cstdint>
#include <string>

namespace net {

class Acceptor {
public:
    Acceptor(const std::string& host, std::uint16_t port);
    ~Acceptor();

    Acceptor(const Acceptor&) = delete;
    Acceptor& operator=(const Acceptor&) = delete;

    int fd() const;
    int acceptClient();

private:
    int listen_fd_{-1};
};

}  // namespace net
