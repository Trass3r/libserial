/**
* (C) Copyright 2017 Trass3r
* Use, modification and distribution are subject to
* the Boost Software License, Version 1.0.
* (see http://www.boost.org/LICENSE_1_0.txt)
*/
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <cassert>
#include <type_traits>

enum class ByteSize : uint8_t
{
	FiveBits = 5,
	SixBits = 6,
	SevenBits = 7,
	EightBits = 8
};

enum class Parity : uint8_t
{
	None = 0,
	Odd = 1,
	Even = 2,
	Mark = 3,
	Space = 4
};

enum class StopBits : uint8_t
{
	One = 0,
	Two = 2,
	One_and_a_half = 1
};

enum class FlowControl : uint8_t
{
	None = 0,
	Software,
	Hardware
};

class Serial final
{
public:
	Serial(const char* port,
		uint32_t baudrate,
		Parity parity = Parity::None,
		StopBits stopbits = StopBits::One,
		FlowControl flowcontrol = FlowControl::None,
		ByteSize bytesize = ByteSize::EightBits);

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
		while (i < len)
		{
			size_t bytesRead = read(buffer + i, 1);
			if (bytesRead == 0)
				break; // Timeout occured on reading 1 byte

			if (buffer[i] == '\n')
			{
				break;
			}

			++i;
		}
		return i;
	}

	using ErrorHandler = void(*)(const char* msg);
	void setErrorHandler(ErrorHandler handler)
	{
		_errorHandler = handler;
	}

	[[nodiscard]]
	bool open();

	bool close();

	bool isOpen() const;

	void setTimeout(uint32_t timeoutMs);

    size_t read(void* buf, size_t size);

	//! always blocks until a byte is read
    uint8_t readByte();

	size_t write(const void* data, size_t length);

	//! convenience function for string literals
	template <size_t N>
	size_t write(const char (&str)[N])
	{
		return write(str, N);
	}

	//! convenience function for structures
	template <typename T,
		typename = std::enable_if_t<std::is_class<std::decay_t<T>>::value>>
	auto write(T&& t)
	{
		return write(&t, sizeof(T));
	}

	uint32_t bytesReady() const;

private:
	void onError() const;
	void configurePort();

	std::string _port;
	ErrorHandler _errorHandler = nullptr;
#ifdef _WIN32
	using native_handle = void*;
#else
	using native_handle = int;
#endif
	native_handle _handle = (native_handle)-1;
	uint32_t _baudrate = 0;
	uint32_t _timeout = 0;
	Parity _parity = Parity::None;
	StopBits _stopbits = StopBits::One;
	FlowControl _flowcontrol = FlowControl::None;
	ByteSize _bytesize = ByteSize::EightBits;
};
