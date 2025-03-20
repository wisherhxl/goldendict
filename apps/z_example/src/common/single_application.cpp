#include "single_application.h"

#include <string>

namespace ti {

SingleApplication::SingleApplication()
#ifdef _WIN32
    : global_mutex_(CreateMutexA(nullptr, true, "TI_TIGER")), valid_(false) {
    if (global_mutex_ != nullptr && GetLastError() != ERROR_ALREADY_EXISTS) {
        valid_ = true;
    }
}
#else
    : lock_file_fd_(-1), valid_(false) {
    const std::string lock_file_path = "/tmp/TI_TIGER.lock";
    lock_file_fd_ = open(lock_file_path.c_str(), O_CREAT | O_RDWR, 0666);
    if (lock_file_fd_ != -1 && flock(lock_file_fd_, LOCK_EX | LOCK_NB) == 0) {
        valid_ = true;
    }
}
#endif

ti::SingleApplication::~SingleApplication() {
#ifdef _WIN32
    if (valid_) {
        ReleaseMutex(global_mutex_);
        CloseHandle(global_mutex_);
    }
#else
    if (lock_file_fd_ != -1) {
        flock(lock_file_fd_, LOCK_UN);
        close(lock_file_fd_);
    }
#endif
}

bool SingleApplication::isValid() const {
    return valid_;
}

}  // namespace ti