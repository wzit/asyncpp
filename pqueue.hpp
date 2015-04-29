#ifndef _PQUEUE_HPP_
#define _PQUEUE_HPP_

#include <vector>
#include <cstdint>
#include <cassert>
//#include <utility>

namespace asyncpp
{

template <typename T>
class pqueue
{
private:
	std::vector<T> data;
	std::vector<int32_t> min_heap; //data对应的最小堆，使用下标索引data中的元素
	std::vector<int32_t> data_to_heap; //data中的元素在heap中的下标
	std::vector<int32_t> free_list; //data中可用位置列表

	void shift_down(int32_t heap_index)
	{
		int32_t heap_idx = heap_index;
		int32_t data_index = min_heap[heap_idx];
		int32_t last_leaf = min_heap.size() - 1;
		int32_t last_non_leaf = (last_leaf - 1) >> 1;
		while (heap_idx <= last_non_leaf)
		{
			int32_t right_child = (heap_idx + 1) << 1;
			int32_t left_child = right_child - 1;
			if (right_child <= last_leaf &&
				data[min_heap[right_child]] < data[min_heap[left_child]])
			{
				left_child = right_child;
			}
			if (data[min_heap[left_child]] < data[data_index])
			{
				min_heap[heap_idx] = min_heap[left_child];
				data_to_heap[min_heap[heap_idx]] = heap_idx;
				heap_idx = left_child;
			}
			else break;
		}
		if (heap_index != heap_idx)
		{
			min_heap[heap_idx] = data_index;
			data_to_heap[data_index] = heap_idx;
		}
#ifdef _ASYNCPP_DEBUG
		assert(check());
#endif
	}

	void shift_up(int32_t heap_index)
	{
		int32_t heap_idx = heap_index;
		int32_t data_index = min_heap[heap_idx];
		int32_t father = (heap_idx - 1) >> 1; // -1/2 = 0; -1>>1 = -1;
		while (father >= 0)
		{
			if (data[data_index] < data[min_heap[father]])
			{
				min_heap[heap_idx] = min_heap[father];
				data_to_heap[min_heap[heap_idx]] = heap_idx;
				heap_idx = father; father = (heap_idx - 1) >> 1;
			}
			else break;
		}
		if (heap_index != heap_idx)
		{
			min_heap[heap_idx] = data_index;
			data_to_heap[data_index] = heap_idx;
		}
#ifdef _ASYNCPP_DEBUG
		assert(check());
#endif
	}

public:
	pqueue() = default;
	~pqueue() = default;
	pqueue(const pqueue& v) = delete;
	pqueue& operator=(const pqueue& v) = delete;

	bool empty()const{return min_heap.empty();}

	uint32_t size()const{return min_heap.size();}

	void reserve(int32_t n)
	{
		data.reserve(n);
		min_heap.reserve(n);
		data_to_heap.reserve(n);
		free_list.reserve(n);
	}

	bool is_index_valid(int32_t data_index) const
	{
		if (static_cast<uint32_t>(data_index) < data_to_heap.size())
		{
			if (data_to_heap[data_index] >= 0) return true;
		}
		return false;
	}

	int32_t push(T&& val)
	{
		int32_t data_index;
		uint32_t size = free_list.size();
		if (size != 0)
		{
			data_index = free_list.back();
			free_list.pop_back();
			data[data_index] = std::move(val);
		}
		else
		{
			data_index = data.size();
			data.push_back(std::move(val));
			data_to_heap.resize(data.size());
		}
		int32_t i = min_heap.size();
		min_heap.push_back(data_index);
		data_to_heap[data_index] = i;
		shift_up(i);
		return data_index;
	}

	void pop()
	{
		int32_t data_index = min_heap[0];
		min_heap[0] = min_heap.back();
		data_to_heap[min_heap[0]] = 0;
		min_heap.pop_back();
		data_to_heap[data_index] = -1;
		if (!min_heap.empty()) shift_down(0);
		free_list.push_back(data_index);
		//return data[data_index];
	}

	const T& front() const { return data[min_heap[0]]; }

	int32_t front_index() const { return min_heap[0]; }

	void change_priority(int32_t data_index)
	{
		int32_t heap_index = data_to_heap[data_index];
		int32_t father = (heap_index - 1) >> 1; // -1/2 = 0; -1>>1 = -1;
		if (father >= 0 && data[data_index] < data[min_heap[father]])
		{
			shift_up(heap_index);
		}
		else
		{
			shift_down(heap_index);
		}
	}

	void remove(int32_t data_index)
	{
		int32_t heap_index = data_to_heap[data_index];
		data_to_heap[data_index] = -1;
		free_list.push_back(data_index);
		data_index = min_heap.back();
		min_heap.pop_back();
		if (heap_index != static_cast<int32_t>(min_heap.size()))
		{ //删除的不是堆的最后一个元素
			int32_t father = (heap_index - 1) >> 1; // -1/2 = 0; -1>>1 = -1;
			min_heap[heap_index] = data_index;
			data_to_heap[data_index] = heap_index;
			if (father > 0 && data[data_index] < data[min_heap[father]])
			{
				shift_up(heap_index);
			}
			else
			{
				shift_down(heap_index);
			}
		}
	}

	const T& operator[](int32_t data_index) const
	{
		return data[data_index];
	}

	T& operator[](int32_t data_index) { return data[data_index]; }

	bool check()
	{
		int32_t heap_idx = 0;
		int32_t last_leaf = min_heap.size() - 1;
		int32_t last_non_leaf = (last_leaf - 1) >> 1;
		for (; heap_idx <= last_non_leaf; ++heap_idx)
		{
			int32_t right_child = (heap_idx + 1) << 1;
			int32_t left_child = right_child - 1;
			if (data[min_heap[left_child]] < data[min_heap[heap_idx]])
			{
				return false;
			}

			if (right_child <= last_leaf &&
				data[min_heap[right_child]] < data[min_heap[heap_idx]])
			{
				return false;
			}
		}
		return true;
	}
};

} //end of namespace asyncpp

#endif
