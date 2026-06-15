#include "core/single_instance_guard.h"

namespace vrperf {

SingleInstanceGuard::SingleInstanceGuard(const wchar_t* mutexName)
{
    mutex_ = CreateMutexW(nullptr, TRUE, mutexName);
    lastError_ = mutex_ ? GetLastError() : GetLastError();
    acquired_ = mutex_ && lastError_ != ERROR_ALREADY_EXISTS;
}

SingleInstanceGuard::~SingleInstanceGuard()
{
    Reset();
}

SingleInstanceGuard::SingleInstanceGuard(SingleInstanceGuard&& other) noexcept
    : mutex_(other.mutex_),
      acquired_(other.acquired_),
      lastError_(other.lastError_)
{
    other.mutex_ = nullptr;
    other.acquired_ = false;
    other.lastError_ = ERROR_SUCCESS;
}

SingleInstanceGuard& SingleInstanceGuard::operator=(SingleInstanceGuard&& other) noexcept
{
    if (this != &other) {
        Reset();
        mutex_ = other.mutex_;
        acquired_ = other.acquired_;
        lastError_ = other.lastError_;
        other.mutex_ = nullptr;
        other.acquired_ = false;
        other.lastError_ = ERROR_SUCCESS;
    }

    return *this;
}

bool SingleInstanceGuard::IsAcquired() const
{
    return acquired_;
}

DWORD SingleInstanceGuard::LastError() const
{
    return lastError_;
}

void SingleInstanceGuard::Reset()
{
    if (!mutex_) {
        return;
    }

    if (acquired_) {
        ReleaseMutex(mutex_);
    }
    CloseHandle(mutex_);
    mutex_ = nullptr;
    acquired_ = false;
}

} // namespace vrperf
