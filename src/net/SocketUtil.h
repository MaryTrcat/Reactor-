#pragma once

namespace net {

int setNonBlocking(int fd);
void closeFd(int fd);

}  // namespace net
