#include "stdafx.h"
#include "Lock.h"

namespace base {

Lock::Lock()
{
    // 2000 指的是在等待进入临界区时自旋个数，防止线程睡眠
    ::InitializeCriticalSectionAndSpinCount(&cs_, 2000);
}

Lock::~Lock()
{
    ::DeleteCriticalSection(&cs_);
}

bool Lock::Try()
{
    if (::TryEnterCriticalSection(&cs_) != FALSE) {
        return true;
    }
    return false;
}

void Lock::Acquire()
{
    ::EnterCriticalSection(&cs_);
}

void Lock::Release()
{
    ::LeaveCriticalSection(&cs_);
}

}
