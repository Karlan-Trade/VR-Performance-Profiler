#pragma once

#include <Windows.h>

#include <string>

namespace vrperf {

class SingleInstanceGuard {
public:
    explicit SingleInstanceGuard(const wchar_t* mutexName);
    ~SingleInstanceGuard();

    SingleInstanceGuard(const SingleInstanceGuard&) = delete;
    SingleInstanceGuard& operator=(const SingleInstanceGuard&) = delete;

    SingleInstanceGuard(SingleInstanceGuard&& other) noexcept;
    SingleInstanceGuard& operator=(SingleInstanceGuard&& other) noexcept;

    bool IsAcquired() const;
    DWORD LastError() const;

private:
    void Reset();

    HANDLE mutex_ = nullptr;
    bool acquired_ = false;
    DWORD lastError_ = ERROR_SUCCESS;
};

} // namespace vrperf
