#include "protocol/Codec.h"

#include <cassert>
#include <iostream>

using protocol::Codec;
using protocol::Frame;

void testRoundTrip() {
    Codec encoder;
    Codec decoder;
    Frame input{3, 42, "hello"};

    const auto bytes = encoder.encode(input);
    decoder.append(bytes.data(), bytes.size());

    const auto output = decoder.tryDecode();
    assert(output.has_value());
    assert(output->message_type == input.message_type);
    assert(output->request_id == input.request_id);
    assert(output->body == input.body);
    assert(decoder.bufferedBytes() == 0);
}

void testPartialPacket() {
    Codec encoder;
    Codec decoder;
    Frame input{5, 7, "partial-body"};

    const auto bytes = encoder.encode(input);
    decoder.append(bytes.data(), 4);
    assert(!decoder.tryDecode().has_value());

    decoder.append(bytes.data() + 4, bytes.size() - 4);
    const auto output = decoder.tryDecode();
    assert(output.has_value());
    assert(output->body == input.body);
}

void testMultiplePackets() {
    Codec encoder;
    Codec decoder;
    const auto first = encoder.encode(Frame{1, 1, "one"});
    const auto second = encoder.encode(Frame{2, 2, "two"});

    decoder.append(first.data(), first.size());
    decoder.append(second.data(), second.size());

    const auto a = decoder.tryDecode();
    const auto b = decoder.tryDecode();
    assert(a.has_value());
    assert(b.has_value());
    assert(a->body == "one");
    assert(b->body == "two");
    assert(!decoder.tryDecode().has_value());
}

int main() {
    testRoundTrip();
    testPartialPacket();
    testMultiplePackets();
    std::cout << "test_codec passed\n";
    return 0;
}
