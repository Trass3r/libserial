/**
* (C) Copyright 2017 Trass3r
* Use, modification and distribution are subject to
* the Boost Software License, Version 1.0.
* (see http://www.boost.org/LICENSE_1_0.txt)
*/
#pragma once

#include <cstdio>
#include <cassert>
#include <cstdint>

class Serial final
{
	FILE* _file = nullptr;
public:
	Serial(const char* file)
	{
		_file = fopen(file, "rb");
	}

	Serial(const Serial&) = delete;
	void operator=(const Serial&) = delete;

	~Serial()
	{
		close();
	}

	size_t readLine(void* buffer_, size_t len)
	{
		uint8_t* buffer = (uint8_t*)buffer_;
		assert(buffer);
		assert(len > 0);

		size_t i = 0;
		// bool seenCR = false;
		while (i < len)
		{
			size_t bytesRead = read(buffer + i, 1);
			if (bytesRead == 0)
				break; // Timeout occured on reading 1 byte

			if (buffer[i] == '\r')
			{
				// seenCR = true;
				// continue;
				break;
			}
			if (buffer[i] == '\n') // TODO: would read one too much || seenCR)
			{
				break;
			}

			// TODO: i += bytesRead;
			++i;
		}
		return i;
	}

	using ErrorHandler = void(*)(const char* msg);
	void setErrorHandler(ErrorHandler)
	{
	}

	[[nodiscard]]
	bool open()
	{
		return _file != nullptr;
	}

	bool close()
	{
		return fclose(_file) == 0;
	}

	bool isOpen() const
	{
		return _file != nullptr;
	}

	size_t read(void* buf, size_t size)
	{
		assert(isOpen());
		return fread(buf, 1, size, _file);
	}

	size_t write(const void* data, size_t length)
	{
		return length;
	}

	//! convenience function for string literals
	template <size_t N>
	size_t write(const char(&str)[N])
	{
		return write(str, N);
	}
};
