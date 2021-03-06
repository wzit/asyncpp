﻿#ifndef _SYNCQUEUE_HPP_
#define _SYNCQUEUE_HPP_

#include <mutex>
#include <cstdint>
#include "spinlock.hpp"

#ifdef _WIN32
#pragma warning(disable:4351) //for visual studio
#endif

namespace asyncpp
{

//typedef std::mutex SyncQueueMutex;
//typedef std::lock_guard<std::mutex> SyncQueueScopeLock;
typedef SpinLock SyncQueueMutex;
typedef ScopeSpinLock SyncQueueScopeLock;

//最多容纳N-1个数据，本对象会占用 N*sizeof(T) 的内存空间
template<typename T, std::size_t N = 128>
class FixedSizeCircleQueue
{
private:
	SyncQueueMutex mtx;
	T queue_data[N];
	uint32_t front_pos;
	uint32_t after_back_pos;
public:
	FixedSizeCircleQueue() :mtx(), queue_data(), front_pos(N - 1), after_back_pos(N - 1){}
	~FixedSizeCircleQueue() = default;
	FixedSizeCircleQueue(const FixedSizeCircleQueue&) = delete;
	FixedSizeCircleQueue& operator=(const FixedSizeCircleQueue&) = delete;

	uint32_t size() const { return front_pos >= after_back_pos ? front_pos - after_back_pos : N - (after_back_pos - front_pos); }
	uint32_t size_safe(){ SyncQueueScopeLock lock(mtx); return size(); }
	bool empty() const { return front_pos == after_back_pos; }
	bool empty_safe(){ SyncQueueScopeLock lock(mtx); return empty(); }
	bool full() const { return after_back_pos != 0 ? after_back_pos - 1 == front_pos : N - 1 == front_pos; }
	bool full_safe(){ SyncQueueScopeLock lock(mtx); return full(); }

	bool push(T&& val)
	{
		SyncQueueScopeLock lock(mtx);
		uint32_t new_pos = after_back_pos != 0 ? after_back_pos - 1 : N - 1;
		if (new_pos != front_pos)
		{
			queue_data[after_back_pos] = std::move(val);
			after_back_pos = new_pos;
			return true;
		}
		else return false;
	}
	bool pop(T&& val)
	{
		SyncQueueScopeLock lock(mtx);
		if (front_pos != after_back_pos)
		{
			uint32_t pos = front_pos;
			front_pos = pos != 0 ? pos - 1 : N - 1;
			val = std::move(queue_data[pos]);
			return true;
		}
		else return false;
	}
	uint32_t pop(T* val, uint32_t n)
	{
		uint32_t cnt = 0;
		SyncQueueScopeLock lock(mtx);
		while (front_pos != after_back_pos && cnt < n)
		{
			uint32_t pos = front_pos;
			front_pos = pos != 0 ? pos - 1 : N - 1;
			val[cnt++] = std::move(queue_data[pos]);
		} 
		return cnt;
	}
	T&& pop()
	{
		SyncQueueScopeLock lock(mtx);
		if (front_pos != after_back_pos)
		{
			uint32_t pos = front_pos;
			front_pos = pos != 0 ? pos - 1 : N - 1;
			return std::move(queue_data[pos]);
		}
		else return std::move(T());
	}

	/*非线程安全*/
	T& front()
	{
		//std::lock_guard<std::mutex> lock(mtx);
		//if (front_pos != after_back_pos)
		return queue_data[front_pos];
	}

	/*遍历全部元素,返回删除的元素个数
	int32_t callback(T& v, void*);
	return 0 -> ignore
	       1 -> remove the element
	*/
	int32_t traverse(int32_t(*callback)(T&, void*), void* arg)
	{
		SyncQueueScopeLock lock(mtx);
		int32_t ret;
		uint32_t pos;
		uint32_t cnt = 0;
		uint32_t new_back_pos = front_pos;

		for (new_back_pos = front_pos;
			new_back_pos != after_back_pos;
			new_back_pos = new_back_pos != 0 ? new_back_pos - 1 : N - 1)
		{
			ret = callback(queue_data[new_back_pos], arg);
			if (ret != 0)
			{
				++cnt;
				if (new_back_pos != after_back_pos) goto L_RM_ELEMENT;
				else goto L_END;
			}
		}
		return 0;

	L_RM_ELEMENT:
		for (pos = new_back_pos != 0 ? new_back_pos - 1 : N - 1;
			pos != after_back_pos;
			pos = pos != 0 ? pos - 1 : N - 1)
		{
			ret = callback(queue_data[pos], arg);
			if (ret != 0)
			{
				++cnt;
			}
			else
			{
				queue_data[new_back_pos] = std::move(queue_data[pos]);
				new_back_pos = new_back_pos != 0 ? new_back_pos - 1 : N - 1;
			}
		}

	L_END:
		after_back_pos = new_back_pos;
		return cnt;
	}
};

} //end of namespace asyncpp

#endif
