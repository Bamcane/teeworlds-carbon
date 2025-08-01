/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef BASE_TL_THREADING_H
#define BASE_TL_THREADING_H

#include "../system.h"

/*
	atomic_inc - should return the value after increment
	atomic_dec - should return the value after decrement
	atomic_compswap - should return the value before the eventual swap
	sync_barrier - creates a full hardware fence
*/

#if defined(__GNUC__)

inline unsigned atomic_inc(volatile unsigned *pValue)
{
	return __sync_add_and_fetch(pValue, 1);
}

inline unsigned atomic_dec(volatile unsigned *pValue)
{
	return __sync_add_and_fetch(pValue, -1);
}

inline unsigned atomic_compswap(volatile unsigned *pValue, unsigned comperand, unsigned value)
{
	return __sync_val_compare_and_swap(pValue, comperand, value);
}

inline void sync_barrier()
{
	__sync_synchronize();
}

#elif defined(_MSC_VER)
#include <intrin.h>
#include <windows.h>

inline unsigned atomic_inc(volatile unsigned *pValue)
{
	return _InterlockedIncrement((volatile long *) pValue);
}

inline unsigned atomic_dec(volatile unsigned *pValue)
{
	return _InterlockedDecrement((volatile long *) pValue);
}

inline unsigned atomic_compswap(volatile unsigned *pValue, unsigned comperand, unsigned value)
{
	return _InterlockedCompareExchange((volatile long *) pValue, (long) value, (long) comperand);
}

inline void sync_barrier()
{
	MemoryBarrier();
}
#else
#error missing atomic implementation for this compiler
#endif

class semaphore
{
	SEMAPHORE sem;

public:
	semaphore() { sphore_init(&sem); }
	~semaphore() { sphore_destroy(&sem); }
	void wait() { sphore_wait(&sem); }
	void signal() { sphore_signal(&sem); }
};

class lock
{
	friend class scope_lock;

	LOCK var;

	void take() { lock_wait(var); }
	void release() { lock_unlock(var); }

public:
	lock()
	{
		var = lock_create();
	}

	~lock()
	{
		lock_destroy(var);
	}
};

class scope_lock
{
	lock *var;

public:
	scope_lock(lock *l)
	{
		var = l;
		var->take();
	}

	~scope_lock()
	{
		var->release();
	}
};

#endif // BASE_TL_THREADING_H
