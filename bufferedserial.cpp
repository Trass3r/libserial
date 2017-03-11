/**
* (C) Copyright 2017 Trass3r
* Use, modification and distribution are subject to
* the Boost Software License, Version 1.0.
* (see http://www.boost.org/LICENSE_1_0.txt)
*/
#include "serial.h"
#include "ringbufferbase.h"
#include <cstring>

class BufferedSerial final
{
	enum { N = 64 * 1024 };
	Serial& _serial;
	RingBufferBase _buffer { N };
	uint8_t* head = _buffer.base;
	size_t bytesAvail = 0;

public:
	BufferedSerial(Serial& serial)
	: _serial(serial)
	{}

	size_t read(void* buf, size_t size)
	{
		_serial.setTimeout(100);

		uint8_t* tail = head + bytesAvail;
		while (size > bytesAvail)
		{
			size_t actuallyRead = _serial.read(tail, N - bytesAvail);
			bytesAvail += actuallyRead;
			tail += actuallyRead;
		}

		memcpy(buf, head, size);
		bytesAvail -= size;
		head += size;
		if (head >= _buffer.base + N)
			head -= N;

		return size;
	}
};
