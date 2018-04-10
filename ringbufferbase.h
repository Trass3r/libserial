/**
* (C) Copyright 2017 Trass3r
* Use, modification and distribution are subject to
* the Boost Software License, Version 1.0.
* (see http://www.boost.org/LICENSE_1_0.txt)
*/
#pragma once

#include <cstdint>
#include <cstdlib>
#include <cassert>

#ifdef _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#else

#include <unistd.h>
#include <linux/memfd.h>
#include <sys/syscall.h>
#include <sys/mman.h>

// not exposed by glibc
static int memfd_create(const char *name, unsigned int flags)
{
	return (int)syscall(__NR_memfd_create, name, flags);
}

#endif

struct SafeHandle final
{
#ifdef _WIN32
	using native_handle = void*;
#else
	using native_handle = int;
#endif
	native_handle handle = 0;

	SafeHandle(native_handle handle) : handle(handle) {}
	SafeHandle(SafeHandle&& o) noexcept
	{
		handle = o.handle;
		o.handle = 0;
	}

	void operator=(SafeHandle&& o) noexcept
	{
		reset();
		handle = o.handle;
		o.handle = 0;
	}

	void reset()
	{
		if (!handle)
			return;

#ifdef _WIN32
		CloseHandle(handle);
#else
		close(handle);
#endif

		handle = 0;
	}

	~SafeHandle()
	{
		reset();
	}

	operator native_handle() const
	{
		return handle;
	}
};

//! simple ring buffer support code leveraging virtual memory tricks
//!
//! it maps the same physical memory twice into a contiguous virtual memory space
struct RingBufferBase final
{
	size_t   physicalSize = 0;
	uint8_t* base = nullptr;
	SafeHandle fileHandle;

	explicit RingBufferBase(size_t bufSize = 64 * 1024)
	{
		alloc(bufSize);
	}

	~RingBufferBase()
	{
		free();
	}

	//! allocate the buffer and return the mapped virtual address
	//! guarantees zero-initialization
	void* alloc(size_t size)
	{
		// called twice
		assert(!base);

		physicalSize = size;

#ifdef _WIN32

		assert(size % 65536 == 0 && "size must be a multiple of 64k");

		// create anonymous file
		SafeHandle anonFile = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, (uint64_t)size >> 32, size & 0xffffffff, nullptr);
		if (!anonFile)
			return nullptr;

		void* ptr = nullptr;
		for (uint32_t i = 0; i < 4; ++i)
		{
			// find memory space large enough for both virtual copies
			ptr = VirtualAlloc(nullptr, 2 * size, MEM_RESERVE, PAGE_NOACCESS);
			if (!ptr)
			{
				free();
				return nullptr;
			}
			VirtualFree(ptr, 0, MEM_RELEASE);

			// map the buffer
			ptr = MapViewOfFileEx(fileHandle, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, size, ptr);
			if (!ptr || !MapViewOfFileEx(fileHandle, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, size, (char *)ptr + size))
				continue;
		}

		base = (uint8_t*)ptr;
		if (!ptr)
		{
			free();
			return nullptr;
		}

		fileHandle = anonFile;
#else
		assert(size % (size_t)getpagesize() == 0);

		// create anonymous file
		fileHandle = memfd_create("ringbuffer", 0);
		if (ftruncate(fileHandle, (__off_t)size) != 0)
			return nullptr;

		// find memory space large enough for both virtual copies
		base = (uint8_t*)mmap(nullptr, 2 * size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

		// map the buffer
		mmap(base, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fileHandle, 0);
		mmap(base + size, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fileHandle, 0);
#endif
		return base;
	}

	//! cleanup
	void free()
	{
		if (base)
		{
#ifdef _WIN32
			UnmapViewOfFile(base);
			UnmapViewOfFile(base + physicalSize);
#else
			munmap(base, 2 * physicalSize);
#endif
			base = nullptr;
		}
		physicalSize = 0;
		fileHandle.reset();
	}
};
