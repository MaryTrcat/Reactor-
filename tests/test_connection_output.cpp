#include "net/TcpConnection.h"
#include "protocol/Codec.h"

#include <arpa/inet.h>
#include <cassert>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <iostream>

int main() {
    int sockets[2] = {-1, -1};
    assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

    net::TcpConnection connection(sockets[0]);
    connection.queueFrame(protocol::Frame{9, 42, "queued-response"});
    assert(connection.hasPendingOutput());
    assert(connection.handleWrite());
    assert(!connection.hasPendingOutput());

    std::uint8_t bytes[128]{};
    const auto received = ::recv(sockets[1], bytes, sizeof(bytes), 0);
    assert(received > 0);

    protocol::Codec codec;
    codec.append(bytes, static_cast<std::size_t>(received));
    const auto frame = codec.tryDecode();
    assert(frame.has_value());
    assert(frame->message_type == 9);
    assert(frame->request_id == 42);
    assert(frame->body == "queued-response");

    ::close(sockets[1]);
    std::cout << "test_connection_output passed\n";
    return 0;
}
