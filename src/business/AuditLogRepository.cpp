#include "business/AuditLogRepository.h"

#include <algorithm>

namespace business {

void AuditQuery::normalize() {
    if (page < 1) {
        page = 1;
    }
    if (page_size <= 0) {
        page_size = kDefaultPageSize;
    }
    if (page_size > kMaxPageSize) {
        page_size = kMaxPageSize;
    }
    if (from_timestamp < 0) {
        from_timestamp = 0;
    }
    if (to_timestamp < 0) {
        to_timestamp = 0;
    }
}

}  // namespace business
