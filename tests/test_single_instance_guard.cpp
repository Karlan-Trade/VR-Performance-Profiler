#include "core/single_instance_guard.h"

#include <Windows.h>

#include <cassert>
#include <iostream>
#include <string>

int main()
{
    const std::wstring mutexName =
        L"Local\\VRPerfProfiler.Test.SingleInstance." +
        std::to_wstring(GetCurrentProcessId());

    {
        vrperf::SingleInstanceGuard first(mutexName.c_str());
        assert(first.IsAcquired());

        vrperf::SingleInstanceGuard second(mutexName.c_str());
        assert(!second.IsAcquired());
        assert(second.LastError() == ERROR_ALREADY_EXISTS);
    }

    vrperf::SingleInstanceGuard third(mutexName.c_str());
    assert(third.IsAcquired());

    std::cout << "[PASS] SingleInstanceGuard tests passed\n";
    return 0;
}
