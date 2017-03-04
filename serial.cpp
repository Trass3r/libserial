/**
* (C) Copyright 2017 Trass3r
* Use, modification and distribution are subject to
* the Boost Software License, Version 1.0.
* (see http://www.boost.org/LICENSE_1_0.txt)
*/

#include "serial.h"
#include "utf8conv.h"

#include <cassert>

// cast enum to basetype
template <typename E>
constexpr auto decay(E e)
{
	return static_cast<std::underlying_type_t<E>>(e);
}

uint8_t Serial::readByte()
{
	uint8_t data = 0;
	[[maybe_unused]]
	size_t bytesRead = read(&data, 1);

	assert(bytesRead == 1 && "readByte returned no data");
	return data;
}

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

Serial::Serial(const char* port, uint32_t baudrate, Parity parity, StopBits stopbits, FlowControl flowcontrol, ByteSize bytesize)
: _port(std::string("\\\\.\\") + port),
  _handle(INVALID_HANDLE_VALUE),
  _baudrate(baudrate),
  _parity(parity),
  _stopbits(stopbits),
  _flowcontrol(flowcontrol),
  _bytesize(bytesize)
{
	(void)open();
}

void Serial::onError() const
{
	if (_errorHandler)
		_errorHandler(windowsErrorString().c_str());
	else
		assert(false && "no error callback set");
}

void Serial::configurePort()
{
	DCB dcbSerialParams = {};
	dcbSerialParams.DCBlength = sizeof(DCB);

	if (!GetCommState(_handle, &dcbSerialParams))
	{
		onError();
	}

	dcbSerialParams.BaudRate = _baudrate;
	dcbSerialParams.ByteSize = decay(_bytesize);
	dcbSerialParams.StopBits = decay(_stopbits);
	dcbSerialParams.Parity   = decay(_parity);

	// setup flowcontrol
	if (_flowcontrol == FlowControl::Software)
	{
		dcbSerialParams.fOutX = true;
		dcbSerialParams.fInX = true;
	}
	else if (_flowcontrol == FlowControl::Hardware)
	{
		dcbSerialParams.fOutxCtsFlow = true;
		dcbSerialParams.fRtsControl = 3;
	}

	// activate settings
	if (!SetCommState(_handle, &dcbSerialParams))
	{
		onError();
		close(); //?
	}

	// setup timeouts
	setTimeout(_timeout);
}

bool Serial::open()
{
	assert(!_port.empty());
	assert(!isOpen());

	_handle = CreateFileA(_port.c_str(),
		GENERIC_READ | GENERIC_WRITE,
	    0,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);

	if (_handle == INVALID_HANDLE_VALUE)
	{
		onError();
		return false;
	}

	configurePort();
	return true;
}

bool Serial::close()
{
	assert(_handle != INVALID_HANDLE_VALUE);

	BOOL ret = CloseHandle(_handle);
	_handle = INVALID_HANDLE_VALUE;
	if (!ret)
	{
		onError();
		return false;
	}
	return true;
}

bool Serial::isOpen() const
{
	return _handle != INVALID_HANDLE_VALUE;
}

void Serial::setTimeout(uint32_t timeoutMs)
{
	_timeout = timeoutMs;

	// setup timeouts
	COMMTIMEOUTS timeout = { 0xFFFFFFFF, timeoutMs, 0, timeoutMs, 0 };
	if (!SetCommTimeouts(_handle, &timeout))
	{
		assert(false && "could not set port timeout");
	}
}

size_t Serial::read(void* data, size_t length)
{
	assert(isOpen());

	DWORD bytesRead;
	if (!ReadFile(_handle, data, static_cast<DWORD>(length), &bytesRead, nullptr))
	{
		onError();
		return 0;
	}
	return bytesRead;
}

size_t Serial::write(const void* data, size_t length)
{
	assert(isOpen());

	DWORD bytesWritten;
	if (!WriteFile(_handle, data, static_cast<DWORD>(length), &bytesWritten, nullptr))
	{
		onError();
		return 0;
	}
	return bytesWritten;
}
#else
#include <fcntl.h> // open() etc.
#include <string.h> // strerror

#include <termios.h> // tcgetattr etc.
#include <unistd.h>
#include <chrono>

Serial::Serial(const char* port, uint32_t baudrate, Parity parity, StopBits stopbits, FlowControl flowcontrol, ByteSize bytesize)
: _port(port),
  _baudrate(baudrate),
  _parity(parity),
  _stopbits(stopbits),
  _flowcontrol(flowcontrol),
  _bytesize(bytesize)
{
	(void)open();
}

void Serial::onError() const
{
 if (_errorHandler)
		_errorHandler(strerror(errno));
	else
		assert(false && "no error callback set");
}

//! returns 0 on error
static speed_t tryMapBaudRate(uint32_t desiredBaudRate)
{
	static const uint32_t baudrates[] = { 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 500000, 576000, 921600, 1000000, 1152000, 1500000, 2000000, 2500000, 3000000, 3500000, 4000000 };
	static const speed_t speeds[] = { B50, B75, B110, B134, B150, B200, B300, B600, B1200, B1800, B2400, B4800, B9600, B19200, B38400, B57600, B115200, B230400, B460800, B500000, B576000, B921600, B1000000, B1152000, B1500000, B2000000, B2500000, B3000000, B3500000, B4000000 };
	static constexpr size_t numSpeeds = sizeof(speeds) / sizeof(speed_t);

	for (size_t i = 0; i < numSpeeds; ++i)
		if (baudrates[i] == desiredBaudRate)
			return speeds[i];
	return 0;
}

void Serial::configurePort()
{
	assert(isOpen());

	// get current options
	termios settings;
	if (tcgetattr(_handle, &settings) == -1)
	{
		onError();
		return;
	}

	// setup raw mode
	cfmakeraw(&settings);

	auto speed = tryMapBaudRate(_baudrate);
	if (speed)
	{
		if (::cfsetspeed(&settings, speed) != 0)
		{
			onError();
			return;
		}
	}
	else
	{
		onError();
		return;
	}

	// setup byte size
	settings.c_cflag &= (tcflag_t) ~CSIZE;
	if (_bytesize == ByteSize::EightBits)
		settings.c_cflag |= CS8;
	else if (_bytesize == ByteSize::SevenBits)
		settings.c_cflag |= CS7;
	else if (_bytesize == ByteSize::SixBits)
		settings.c_cflag |= CS6;
	else
		settings.c_cflag |= CS5;

	// setup stopbits
	if (_stopbits == StopBits::One)
		settings.c_cflag &= (tcflag_t) ~CSTOPB;
	else
		settings.c_cflag |= CSTOPB;

	// setup parity
	settings.c_iflag &= (tcflag_t) ~(INPCK | ISTRIP);
	if (_parity == Parity::None)
		settings.c_cflag &= (tcflag_t) ~(PARENB | PARODD);
	else if (_parity == Parity::Even)
	{
		settings.c_cflag &= (tcflag_t) ~PARODD;
		settings.c_cflag |=  (PARENB);
	}
	else if (_parity == Parity::Odd)
		settings.c_cflag |=  (PARENB | PARODD);
	else if (_parity == Parity::Mark)
		settings.c_cflag |=  (PARENB | CMSPAR | PARODD);
	else if (_parity == Parity::Space)
	{
		settings.c_cflag |=  (PARENB | CMSPAR);
		settings.c_cflag &= (tcflag_t) ~PARODD;
	}

	// setup flow control
	if (_flowcontrol == FlowControl::Software)
		settings.c_iflag |=  (IXON | IXOFF);
	else
		settings.c_iflag &= (tcflag_t) ~(IXON | IXOFF | IXANY);

#ifdef CRTSCTS
	tcflag_t flag = CRTSCTS;
#elif CNEW_RTSCTS
	tcflag_t flag = CNEW_RTSCTS;
#endif
	if (_flowcontrol == FlowControl::Hardware)
		settings.c_cflag |= flag;
	else
		settings.c_cflag &= ~flag;

	settings.c_cc[VMIN] = 1;
	settings.c_cc[VTIME] = 0;

	// activate settings
	::tcsetattr(_handle, TCSANOW, &settings);
}

bool Serial::open()
{
	assert(!_port.empty());
	assert(!isOpen());

	// open in blocking mode
	_handle = ::open(_port.c_str(), O_RDWR | O_NOCTTY);
	if (_handle == -1)
	{
		if (errno == EINTR)
			return open();

		onError();
		return false;
	}

	configurePort();
	return true;
}

bool Serial::close()
{
	assert(isOpen());

	int ret = ::close(_handle);
	_handle = -1;
	if (!ret)
		return true;

	onError();
	return false;
}

bool Serial::isOpen() const
{
	return _handle != -1;
}

void Serial::setTimeout(uint32_t timeoutMs)
{
	_timeout = timeoutMs;
}

size_t Serial::read(void* data, size_t length)
{
	assert(isOpen());

	size_t bytesRead = 0;
	std::chrono::steady_clock::time_point start;
	// avoid system call if no timeout
	if (_timeout > 0)
		start = std::chrono::steady_clock::now();

	while (bytesRead < length)
	{
		ssize_t n = ::read(_handle, (uint8_t*)data + bytesRead, length - bytesRead);
		if (n > 0)
			bytesRead += (size_t)n;
		else
			onError();

		if (_timeout == 0)
			continue;

		auto diff = std::chrono::steady_clock::now() - start;
		auto msecs = std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
		if (msecs >= _timeout)
			break;
	}

	return bytesRead;
}

size_t Serial::write(const void* data, size_t length)
{
	assert(isOpen());

	size_t bytesWritten = 0;
	while (bytesWritten < length)
	{
		ssize_t n = ::write(_handle, (const uint8_t*)data + bytesWritten, length - bytesWritten);
		if (n > 0)
			bytesWritten += (size_t)n;
		else
			onError();
	}
	return bytesWritten;
}

#endif
