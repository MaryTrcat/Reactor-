#include "net/Acceptor.h"

#include "net/SocketUtil.h"

#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace net {

Acceptor::Acceptor(const std::string& host, std::uint16_t port) {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        throw std::runtime_error("socket failed");
    }

    int reuse = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) <= 0) {
        closeFd(listen_fd_);
        throw std::runtime_error("invalid listen host: " + host);
    }

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        closeFd(listen_fd_);
        throw std::runtime_error("bind failed");
    }
    if (listen(listen_fd_, SOMAXCONN) < 0) {
        closeFd(listen_fd_);
        throw std::runtime_error("listen failed");
    }
    if (setNonBlocking(listen_fd_) < 0) {
        closeFd(listen_fd_);
        throw std::runtime_error("set listen fd non-blocking failed");
    }
}

Acceptor::~Acceptor() {
    closeFd(listen_fd_);
}

int Acceptor::fd() const {
    return listen_fd_;
}

int Acceptor::acceptClient() {
    sockaddr_in client_address{};
    socklen_t length = sizeof(client_address);
    const int client_fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_address), &length);
    if (client_fd >= 0) {
        setNonBlocking(client_fd);
    }
    return client_fd;
}

}  // namespace net
