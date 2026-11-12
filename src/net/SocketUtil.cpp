#include "net/SocketUtil.h"

#include <fcntl.h>
#include <unistd.h>

namespace net {

int setNonBlocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void closeFd(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

}  // namespace net
