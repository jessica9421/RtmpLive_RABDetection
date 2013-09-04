/*******************************************************************************
 * waitable_event.h
 * Copyright: (c) 2013 Haibin Du(haibinnet@qq.com). All rights reserved.
 * -----------------------------------------------------------------------------
 *
 *
 *
 * -----------------------------------------------------------------------------
 * 2013-4-13 - Created (Haibin Du)
 ******************************************************************************/

#ifndef _BASE_WAIT_EVENT_
#define _BASE_WAIT_EVENT_

#include "Base.h"

namespace base {

class WaitableEvent
{
 public:
  // If manual_reset is true, then to set the event state to non-signaled, a
  // consumer must call the Reset method.  If this parameter is false, then the
  // system automatically resets the event state to non-signaled after a single
  // waiting thread has been released.
  WaitableEvent(bool manual_reset, bool initially_signaled);

  ~WaitableEvent();

  void Reset();

  void Signal();

  // Returns true if the event is in the signaled state, else false.  If this
  // is not a manual reset event, then this test will cause a reset.
  bool IsSignaled();

  void Wait();

  bool TimedWait(unsigned int millsecs);

  HANDLE handle() const { return handle_; }

  // Wait, synchronously, on multiple events.
  //   waitables: an array of WaitableEvent pointers
  //   count: the number of elements in @waitables
  //
  // returns: the index of a WaitableEvent which has been signaled.
  //
  // You MUST NOT delete any of the WaitableEvent objects while this wait is
  // happening.
  static size_t WaitMany(WaitableEvent** waitables, size_t count);

 private:
  friend class WaitableEventWatcher;

  HANDLE handle_;

  DISALLOW_COPY_AND_ASSIGN(WaitableEvent);
};

}  // namespace hbase

#endif  // _BASE_WAIT_EVENT_
