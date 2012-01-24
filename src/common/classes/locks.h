/*
 *	PROGRAM:		Client/Server Common Code
 *	MODULE:			locks.h
 *	DESCRIPTION:	Single-state locks
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Nickolay Samofatov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2004 Nickolay Samofatov <nickolay@broadviewsoftware.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef CLASSES_LOCKS_H
#define CLASSES_LOCKS_H

#include "firebird.h"
#include "../common/gdsassert.h"

#ifdef WIN_NT
// It is relatively easy to avoid using this header. Maybe do the same stuff like
// in thd.h ? This is Windows platform maintainers choice
#include <windows.h>
#else
#include <pthread.h>
#include <errno.h>
#endif

namespace Firebird {

class MemoryPool;	// Needed for ctors that must always ignore it
class Exception;	// Needed for catch

#ifdef WIN_NT

// Generic process-local mutex and spinlock. The latter
// is used to manage memory heaps in a threaded environment.

// Windows version of the class

class TryEnterCS
{
public:
	TryEnterCS();

	static bool tryEnter(LPCRITICAL_SECTION lpCS)
	{
		return ((*m_funct) (lpCS) == TRUE);
	}

private:
	typedef WINBASEAPI BOOL WINAPI tTryEnterCriticalSection
		(LPCRITICAL_SECTION lpCriticalSection);

	static tTryEnterCriticalSection* m_funct;
};

class Mutex
{
protected:
	CRITICAL_SECTION spinlock;
#ifdef DEV_BUILD
	const char* reason;
	int lockCount;
#endif

public:
	Mutex()
#ifdef DEV_BUILD
		: reason(NULL), lockCount(0)
#endif
	{
		InitializeCriticalSection(&spinlock);
	}
	explicit Mutex(MemoryPool&)
#ifdef DEV_BUILD
		: reason(NULL), lockCount(0)
#endif
	{
		InitializeCriticalSection(&spinlock);
	}

	~Mutex()
	{
#if defined DEV_BUILD && !defined WIN9X_SUPPORT
		if (spinlock.OwningThread != 0)
			DebugBreak();
		fb_assert(lockCount == 0);
#endif
		DeleteCriticalSection(&spinlock);
	}

#ifdef DEV_BUILD
	void enter(const char* aReason)
	{
		EnterCriticalSection(&spinlock);
		reason = aReason;
		lockCount++;
	}
#endif

	void enter()
	{
		EnterCriticalSection(&spinlock);
#ifdef DEV_BUILD
		reason = "<..unspecified..>";
		lockCount++;
#endif
	}

	bool tryEnter()
	{
		const bool ret = TryEnterCS::tryEnter(&spinlock);
#ifdef DEV_BUILD
		if (ret)
		{
			reason = "<..unspecified..>";
			lockCount++;
		}
#endif
		return ret;
	}

	void leave()
	{
#if defined DEV_BUILD && !defined WIN9X_SUPPORT
		// NS: This check is based on internal structure on CRITICAL_SECTION
		// On 9X it works differently, and future OS versions may break this check as well
		if ((U_IPTR) spinlock.OwningThread != GetCurrentThreadId())
			DebugBreak();

		lockCount--;
#endif
		LeaveCriticalSection(&spinlock);
	}

	void assertLocked()
	{
#ifdef DEV_BUILD
		// first of all try to enter the mutex
		// this will help to make sure it's not locked by other thread
		if (!tryEnter())
		{
			fb_assert(false);
		}
		// make sure mutex was already locked prior assertLocked
		fb_assert(lockCount > 1);
		// leave to release lock, done by us in tryEnter
		leave();
#endif
	}

public:
	static void initMutexes() { }
};

class Spinlock : public Mutex
{
private:
	void init();

public:
	Spinlock()
	{
		init();
	}

	explicit Spinlock(MemoryPool&)
	{
		init();
	}
};

#else //WIN_NT

// Pthreads version of the class
class Mutex
{
friend class Condition;
private:
	pthread_mutex_t mlock;
	static pthread_mutexattr_t attr;
#ifdef DEV_BUILD
	const char* reason;
	int lockCount;
#endif

private:
	void init()
	{
#ifdef DEV_BUILD
		reason = NULL;
		lockCount = 0;
#endif
		int rc = pthread_mutex_init(&mlock, &attr);
		if (rc)
			system_call_failed::raise("pthread_mutex_init", rc);
	}

public:
	Mutex() { init(); }
	explicit Mutex(MemoryPool&) { init(); }

	~Mutex()
	{
		fb_assert(lockCount == 0);
		int rc = pthread_mutex_destroy(&mlock);
		if (rc)
			system_call_failed::raise("pthread_mutex_destroy", rc);
	}

#ifdef DEV_BUILD
	void enter(const char* aReason)
	{
		int rc = pthread_mutex_lock(&mlock);
		if (rc)
			system_call_failed::raise("pthread_mutex_lock", rc);
		reason = aReason;
		++lockCount;
	}
#endif

	void enter()
	{
		int rc = pthread_mutex_lock(&mlock);
		if (rc)
			system_call_failed::raise("pthread_mutex_lock", rc);
#ifdef DEV_BUILD
		reason = "<..unspecified..>";
		++lockCount;
#endif
	}

	bool tryEnter()
	{
		int rc = pthread_mutex_trylock(&mlock);
		if (rc == EBUSY)
			return false;
		if (rc)
			system_call_failed::raise("pthread_mutex_trylock", rc);
#ifdef DEV_BUILD
		reason = "<..unspecified..>";
		++lockCount;
#endif
		return true;
	}

	void leave()
	{
#ifdef DEV_BUILD
		fb_assert(lockCount > 0);
		--lockCount;
#endif
		int rc = pthread_mutex_unlock(&mlock);
		if (rc)
		{
#ifdef DEV_BUILD
			++lockCount;
#endif
			system_call_failed::raise("pthread_mutex_unlock", rc);
		}
	}

	void assertLocked()
	{
#ifdef DEV_BUILD
		// first of all try to enter the mutex
		// this will help to make sure it's not locked by other thread
		if (!tryEnter())
		{
			fb_assert(false);
		}
		// make sure mutex was already locked prior assertLocked
		fb_assert(lockCount > 1);
		// leave to release lock, done by us in tryEnter
		leave();
#endif
	}

public:
	static void initMutexes();
};

#ifdef NOT_USED_OR_REPLACED		// we do not use spinlocks currently
class Spinlock
{
private:
	pthread_spinlock_t spinlock;
public:
	Spinlock()
	{
		if (pthread_spin_init(&spinlock, false))
			system_call_failed::raise("pthread_spin_init");
	}

	explicit Spinlock(MemoryPool&)
	{
		if (pthread_spin_init(&spinlock, false))
			system_call_failed::raise("pthread_spin_init");
	}

	~Spinlock()
	{
		if (pthread_spin_destroy(&spinlock))
			system_call_failed::raise("pthread_spin_destroy");
	}

	void enter()
	{
		if (pthread_spin_lock(&spinlock))
			system_call_failed::raise("pthread_spin_lock");
	}

	void leave()
	{
		if (pthread_spin_unlock(&spinlock))
			system_call_failed::raise("pthread_spin_unlock");
	}
};
#else
typedef Mutex Spinlock;
#endif

#endif //WIN_NT


#ifdef DEV_BUILD
#define MLG_REASON aReason
#else
#define MLG_REASON
#endif

// RAII holder
class MutexLockGuard
{
public:
	MutexLockGuard(Mutex &aLock, const char* MLG_REASON)
		: lock(&aLock)
	{
		lock->enter(MLG_REASON);
	}

	explicit MutexLockGuard(Mutex &aLock)
		: lock(&aLock)
	{
		lock->enter();
	}

	~MutexLockGuard()
	{
		try
		{
			lock->leave();
		}
		catch (const Exception&)
		{
			DtorException::devHalt();
		}
	}

private:
	// Forbid copying
	MutexLockGuard(const MutexLockGuard&);
	MutexLockGuard& operator=(const MutexLockGuard&);

	Mutex* lock;
};

#undef MLG_REASON

class MutexUnlockGuard
{
public:
	explicit MutexUnlockGuard(Mutex &aLock)
		: lock(&aLock)
	{
		try
		{
			lock->leave();
		}
		catch (const Exception&)
		{
			DtorException::devHalt();
		}
	}

	~MutexUnlockGuard()
	{
		lock->enter();
	}

private:
	// Forbid copying
	MutexUnlockGuard(const MutexUnlockGuard&);
	MutexUnlockGuard& operator=(const MutexUnlockGuard&);

	Mutex* lock;
};


class MutexCheckoutGuard
{
public:
	MutexCheckoutGuard(Mutex& mtxCout, Mutex& mtxLock) :
		unlock(mtxCout),
		lock(mtxLock)
	{
	}

private:
	MutexUnlockGuard unlock;
	MutexLockGuard	lock;
};

} //namespace Firebird

#endif // CLASSES_LOCKS_H
