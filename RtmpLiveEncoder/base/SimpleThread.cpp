#include "stdafx.h"
#include "SimpleThread.h"

namespace base {

DWORD __stdcall ThreadFunc(void* params)
{
    SimpleThread* thread = static_cast<SimpleThread*>(params);
    thread->ThreadMain();
    return NULL;
}

SimpleThread::SimpleThread()
    : is_stop_(true)
{

}

SimpleThread::~SimpleThread()
{

}

void SimpleThread::Start()
{
    thread_id_ = ::CreateThread(NULL, 0, ThreadFunc, this, 0, NULL);
    //cond_.Wait();    
}

void SimpleThread::Stop()
{
    is_stop_ = true;
}

bool SimpleThread::IsStop()
{
    return is_stop_;
}

void SimpleThread::Join()
{
    ::WaitForSingleObject(thread_id_, INFINITE);
    ::CloseHandle(thread_id_);
}

void SimpleThread::ThreadMain()
{
    is_stop_ = false;
    Run();
}

} // namespace base
