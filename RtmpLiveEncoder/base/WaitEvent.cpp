/*******************************************************************************
 * waitable_event_win.cpp
 * Copyright: (c) 2013 Haibin Du(haibinnet@qq.com). All rights reserved.
 *
 * -----------------------------------------------------------------------------
 * 2013-4-13 - Created (Haibin Du)
 ******************************************************************************/

#include "stdafx.h"
#include "WaitEvent.h"

#include <math.h>

namespace base {

WaitableEvent::WaitableEvent(bool manual_reset, bool signaled)
    : handle_(CreateEvent(NULL, manual_reset, signaled, NULL))
{

}

WaitableEvent::~WaitableEvent()
{
  CloseHandle(handle_);
}

void WaitableEvent::Reset()
{
  ResetEvent(handle_);
}

void WaitableEvent::Signal()
{
  SetEvent(handle_);
}

bool WaitableEvent::IsSignaled()
{
  return TimedWait(0);
}

void WaitableEvent::Wait()
{
  DWORD result = WaitForSingleObject(handle_, INFINITE);
}

bool WaitableEvent::TimedWait(unsigned int millsecs)
{
  DWORD result = WaitForSingleObject(handle_, static_cast<DWORD>(millsecs));
  switch (result)
  {
    case WAIT_OBJECT_0:
      return true;
    case WAIT_TIMEOUT:
      return false;
  }
  return false;
}

// static
size_t WaitableEvent::WaitMany(WaitableEvent** events, size_t count)
{
  HANDLE handles[MAXIMUM_WAIT_OBJECTS];

  for (size_t i = 0; i < count; ++i)
    handles[i] = events[i]->handle();

  DWORD result =
      WaitForMultipleObjects(static_cast<DWORD>(count),
                             handles,
                             FALSE,      // don't wait for all the objects
                             INFINITE);  // no timeout
  if (result >= WAIT_OBJECT_0 + count)
  {
    return 0;
  }

  return result - WAIT_OBJECT_0;
}

}  // namespace hbase
