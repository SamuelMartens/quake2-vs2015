#pragma once

#include <list>
#include <vector>
#include <limits>
#include <d3d12.h>
#include <memory>
#include <cassert>
#include <algorithm>


#include "dx_common.h"

struct Allocation
{
	static constexpr int INVALID_OFFSET = -1;

	int offset = INVALID_OFFSET;
	int size = -1;
};

template<int SIZE>
class BufferAllocator
{
public:



	BufferAllocator() = default;

	BufferAllocator(const BufferAllocator&) = delete;
	BufferAllocator(BufferAllocator&&) = delete;

	BufferAllocator& operator=(const BufferAllocator&) = delete;
	BufferAllocator& operator=(BufferAllocator&&) = delete;

	~BufferAllocator() = default;

	int Allocate(int size) 
	{
		// Check before existing allocations
		{
			const int nextOffset = allocations.empty() ? SIZE : allocations.begin()->offset;

			if (nextOffset >= size)
			{
				allocations.push_front({ 0, size });
			}
		}

		// Check between existing allocations
		{
			const int numIntervals = allocations.size() - 1;

			auto currAllocIt = allocations.begin();
			auto nextAllocIt = std::next(currAllocIt, 1);

			for (int i = 0; i < numIntervals; ++i, ++currAllocIt, ++nextAllocIt)
			{
				const int currAllocEnd = currAllocIt->offset + currAllocIt->size;
				if (nextAllocIt->offset - currAllocEnd >= size)
				{
					allocations.insert(currAllocIt, { currAllocEnd, size });
					return currAllocEnd;
				}
			}
		}

		// Check after existing allocations
		{
			// We checked empty case in the beginning of the function
			if (!allocations.empty())
			{
				const Allocation& lastAlloc = allocations.back();
				const int lastAllocEnd = lastAlloc.offset + lastAlloc.size;

				if (SIZE - lastAllocEnd >= size)
				{
					allocations.push_back({ lastAllocEnd, size });
					return lastAllocEnd;
				}
			}

			assert(false && "Failed to allocate part of buffer");
			return Allocation::INVALID_OFFSET;

		}
	};


	void Delete(int offset) 
	{
		auto it = std::find_if(allocations.begin(), allocations.end(), [offset](const Allocation& alloc)
		{
			return offset == alloc.offset;
		});

		if (it == allocations.end())
		{
			assert(false && "Trying to delete memory that was allocated.");
			return;
		}

		allocations.erase(it);
	};

	void ClearAll() 
	{
		allocations.clear();
	};

private:

	std::list<Allocation> allocations;
};

template<int SIZE>
struct AllocBuffer
{
	AllocBuffer() = default;

	AllocBuffer(const AllocBuffer&) = delete;
	AllocBuffer(AllocBuffer&&) = delete;

	AllocBuffer& operator=(const AllocBuffer&) = delete;
	AllocBuffer& operator=(AllocBuffer&&) = delete;

	// No need to explicitly free our gpu buffer. COM interface will release 
	// gpu memory
	~AllocBuffer() = default;

	BufferAllocator<SIZE> allocator;
	ComPtr<ID3D12Resource> gpuBuffer;
};




using BufferHandler = uint32_t;
//#DEBUG INVALID_HANDLER is so generic. Collision chance is so HIGH
extern const BufferHandler INVALID_BUFFER_HANDLER;


template<int BUFFER_SIZE, int HANDLERS_NUM>
class HandlerBuffer
{
public:

	HandlerBuffer()
		: m_handlers(HANDLERS_NUM, Allocation::INVALID_OFFSET)
	{};

	HandlerBuffer(const HandlerBuffer&) = delete;
	HandlerBuffer(HandlerBuffer&&) = delete;

	HandlerBuffer& operator=(const HandlerBuffer&) = delete;
	HandlerBuffer& operator=(HandlerBuffer&&) = delete;

	~HandlerBuffer() = default;

	BufferHandler Allocate(int size)
	{
		BufferHandler handler = INVALID_BUFFER_HANDLER;
		// Find free handler slot
		for (BufferHandler currentH = 0; currentH < HANDLERS_NUM; ++currentH)
		{
			if (m_handlers[currentH] == Allocation::INVALID_OFFSET)
			{
				handler = currentH;
				break;
			}
		}

		assert(handler != INVALID_BUFFER_HANDLER);

		m_handlers[handler] = allocBuffer.allocator.Allocate(size);

		return handler;
	}

	void Delete(BufferHandler handler)
	{
		assert(m_handlers[handler] != Allocation::INVALID_OFFSET);

		allocBuffer.allocator.Delete(m_handlers[handler]);

		m_handlers[handler] = Allocation::INVALID_OFFSET;
	}

	// IMPORTANT: handler is intentional layer of abstraction between offset and
	// the one who asked for memory allocation. Don't rely on it being the same long term,
	// keep handler around, ask for offset when you need it.
	int GetOffset(BufferHandler handler) const
	{
		assert(m_handlers[handler] != Allocation::INVALID_OFFSET);
		return m_handlers[handler];
	}

	AllocBuffer<BUFFER_SIZE> allocBuffer;

private:

	std::vector<int> m_handlers;

};