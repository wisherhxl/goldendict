#include "single_application.h"

namespace ti
{
    SingleApplication::SingleApplication()
        : global_mutex_(std::make_unique<HANDLE>(CreateMutexA(nullptr, true, "SILT_TIGER"))),
          valid_(false)
    {
        if (global_mutex_ != nullptr && GetLastError() != ERROR_ALREADY_EXISTS)
        {
            valid_ = true;
        }
    }

    SingleApplication::~SingleApplication()
    {
        if (valid_)
        {
            ReleaseMutex(*global_mutex_);
            CloseHandle(*global_mutex_);
        }
    }

    bool SingleApplication::isValid() const
    {
        return valid_;
    }
}
