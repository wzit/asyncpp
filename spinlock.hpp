#ifndef _SPINLOCK_HPP_
#define _SPINLOCK_HPP_

#include <atomic>

#ifdef _WIN32
#include <Winsock2.h> //include before windows.h
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace asyncpp
{

class SpinLock
{
public:
	SpinLock() : m_lock()
	{
#ifdef _WIN32
		//vc bug
		m_lock._My_flag = 0;
#endif
	}
	~SpinLock()
	{
	}
	SpinLock(const SpinLock& r) = delete;
	SpinLock& operator=(const SpinLock& r) = delete;
public:
	//block until obtain the lock
	void lock()
	{
		while (m_lock.test_and_set())
		{
#ifdef _WIN32
			Sleep(1);
#else
			usleep(1);
#endif
		}
	}

	//return true if success to get the lock, false if failed
	bool trySpinLock()
	{
		return !m_lock.test_and_set();
	}

	void unlock()
	{
		m_lock.clear();
	}
private:
	volatile std::atomic_flag m_lock;
};

class ScopeSpinLock{
public:
	ScopeSpinLock(SpinLock& lock)
		: m_plock(&lock)
	{
		m_plock->lock();
	}
	ScopeSpinLock(SpinLock* plock)
		: m_plock(plock)
	{
		m_plock->lock();
	}
	~ScopeSpinLock()
	{
		m_plock->unlock();
	}
	ScopeSpinLock(const ScopeSpinLock&) = delete;
	ScopeSpinLock& operator=(const ScopeSpinLock& r) = delete;
private:
	SpinLock* m_plock;
};

} //end of namespace asyncpp

#endif
