// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SRC_UTILS_VMPI_THREADHELPERS_H_
#define SRC_UTILS_VMPI_THREADHELPERS_H_

#include "tier1/utllinkedlist.h"

#ifdef _WIN64
constexpr inline size_t SIZEOF_CS{40};  // sizeof( CRITICAL_SECTION )
#else
constexpr inline size_t SIZEOF_CS{24};  // sizeof( CRITICAL_SECTION )
#endif

class CCriticalSection {
 public:
  CCriticalSection(unsigned spin_before_lock = 2000U);
  ~CCriticalSection();

 protected:
  friend class CCriticalSectionLock;

  void Lock();
  void Unlock();

 public:
  char m_CS[SIZEOF_CS];

  // Used to protect against deadlock in debug mode.
  // #if defined( _DEBUG )
  CUtlLinkedList<unsigned long, int> m_Locks;

  char m_DeadlockProtect[SIZEOF_CS];
  // #endif
};

// Use this to lock a critical section.
class CCriticalSectionLock {
 public:
  CCriticalSectionLock(CCriticalSection *pCS);
  ~CCriticalSectionLock();

  void Lock();
  void Unlock();

 private:
  CCriticalSection *m_pCS;
  bool m_bLocked;
};

template <typename T>
class CCriticalSectionData : private CCriticalSection {
 public:
  // You only have access to the data between Lock() and Unlock().
  T *Lock() {
    CCriticalSection::Lock();
    return &m_Data;
  }

  void Unlock() { CCriticalSection::Unlock(); }

 private:
  T m_Data;
};

class CEvent {
 public:
  CEvent();
  ~CEvent();

  bool Init(bool bManualReset, bool bInitialState);
  void Term();

  void *GetEventHandle() const;

  // Signal the event.
  bool SetEvent();

  // Unset the event's signalled status.
  bool ResetEvent();

 private:
  void *m_hEvent;
};

#endif  // !SRC_UTILS_VMPI_THREADHELPERS_H_
