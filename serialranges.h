/**
* (C) Copyright 2017 Trass3r
* Use, modification and distribution are subject to
* the Boost Software License, Version 1.0.
* (see http://www.boost.org/LICENSE_1_0.txt)
*/
#pragma once

#include "serial.h"

#if _MSC_VER
#include <string>
using std::string_view;
#else
#include <experimental/string_view>
using std::experimental::string_view;
#endif

//! very minimal dummy iterator to support range-based for loop
template <typename Range>
struct DummyRangeIterator
{
	void operator++()
	{
		range->next();
	}

	decltype(auto) operator*() const
	{
		return range->front();
	}

	bool operator!=(DummyRangeIterator o) const
	{
		return !range->empty();
	}

	Range* range = nullptr;
};


//! unbuffered bytewise access
struct BytesRange
{
	using Iterator = DummyRangeIterator<BytesRange>;

	explicit BytesRange(Serial& serial)
	: serial(serial)
	{
		serial.read(&buffer, 1);
	}

	Iterator begin()
	{
		return Iterator{ this };
	}

	static Iterator end()
	{
		return Iterator{};
	}

	void next()
	{
		serial.read(&buffer, 1);
	}

	uint8_t front() const
	{
		return buffer;
	}

	static bool empty()
	{
		return false;
	}

	Serial& serial;
	uint8_t buffer = 0;
};

//! buffered bytewise access
struct BufferedBytesRange
{
	using Iterator = DummyRangeIterator<BufferedBytesRange>;

	explicit BufferedBytesRange(Serial& serial, uint8_t* buffer, size_t bufsize)
	: serial(serial),
	  buffer(buffer),
	  bufsize(bufsize)
	{
		serial.read(buffer, bufsize);
	}

	Iterator begin()
	{
		return Iterator{ this };
	}

	static Iterator end()
	{
		return Iterator{};
	}

	void next()
	{
		if (index < bufsize)
		{
			++index;
			return;
		}

		serial.read(buffer, bufsize);
		index = 0;
	}

	uint8_t front() const
	{
		return buffer[index];
	}

	static bool empty()
	{
		return false;
	}

	Serial& serial;
	uint8_t* buffer = nullptr;
	size_t bufsize = 0;
	size_t index = 0;
};

template <size_t N>
inline BufferedBytesRange bufferedBytesView(Serial& serial, uint8_t (&buffer)[N])
{
	return BufferedBytesRange(serial, buffer, N);
}

inline BufferedBytesRange bufferedBytesView(Serial& serial, uint8_t* buffer, size_t bufsize)
{
	return BufferedBytesRange(serial, buffer, bufsize);
}

// avoid template just for that
#ifndef MAX_LINE_LEN
#define MAX_LINE_LEN 4096
#endif

//! provides linewise access
struct LinesRange
{
	using Iterator = DummyRangeIterator<LinesRange>;

	explicit LinesRange(BufferedBytesRange input)
	: input(input)
	{
	}

	Iterator begin()
	{
		return {this};
	}

	static Iterator end()
	{
		return {};
	}

	void next()
	{

	}

    string_view front() const
	{
		return {nullptr, 0};
	}

	bool empty() const
	{
		return readBytes > maxBytes;
	}

	BufferedBytesRange input;
	uint8_t linebuffer[MAX_LINE_LEN] = {};
	uint8_t index = 0;
	uint32_t readBytes = 0;
	uint32_t maxBytes = 0;
};

inline LinesRange linesView(BufferedBytesRange input)
{
	return LinesRange(input);
}

inline size_t readLine(Serial& serial, uint8_t* buffer, size_t len)
{
	assert(buffer);
	assert(len > 0);

	size_t i = 0;
	// bool seenCR = false;
	while (i < len)
	{
		size_t bytesRead = serial.read(buffer + i, 1);
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

#if 0
/*!
 * Structure that describes a serial device.
 */
struct PortInfo
{
  //! Address of the serial port (this can be passed to the constructor of Serial)
  std::string port;

  //! Human readable description of serial device if available.
  std::string description;

  //! Hardware ID (e.g. VID:PID of USB serial devices) or "n/a" if not available.
  std::string hardwareId;
};

//! for enumeration
struct SerialPortsRange
{
    using Iterator = DummyRangeIterator<SerialPortsRange>;

    explicit SerialPortsRange(Serial& serial)
    : serial(serial)
    {
        serial.read(&buffer, 1);
    }

    Iterator begin()
    {
        return Iterator{ this };
    }

    static Iterator end()
    {
        return Iterator{};
    }

    void next()
    {
        serial.read(&buffer, 1);
    }

    uint8_t front() const
    {
        return buffer;
    }

    static bool empty()
    {
        return false;
    }

    Serial& serial;
    uint8_t buffer = 0;
};

SerialPortsRange listPorts();

#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
bool list_ports(struct sp_port ***list)
{
    char name[4096], target[4096];
    dirent* entry;
#ifdef HAVE_STRUCT_SERIAL_STRUCT
    struct serial_struct serial_info;
    int ioctl_result;
#endif
    char buf[sizeof(entry->d_name) + 23];
    int len, fd;
    DIR *dir;
    struct stat statbuf;

    if (!(dir = opendir("/sys/class/tty")))
        return false;

    while ((entry = readdir(dir)))
    {
        snprintf(buf, sizeof(buf), "/sys/class/tty/%s", entry->d_name);
        if (lstat(buf, &statbuf) == -1)
            continue;
        if (!S_ISLNK(statbuf.st_mode))
            snprintf(buf, sizeof(buf), "/sys/class/tty/%s/device", entry->d_name);
        len = readlink(buf, target, sizeof(target));
        if (len <= 0 || len >= (int)(sizeof(target) - 1))
            continue;
        target[len] = 0;
        if (strstr(target, "virtual"))
            continue;
        snprintf(name, sizeof(name), "/dev/%s", entry->d_name);
        //DEBUG_FMT("Found device %s", name);
        if (strstr(target, "serial8250")) {
            /*
             * The serial8250 driver has a hardcoded number of ports.
             * The only way to tell which actually exist on a given system
             * is to try to open them and make an ioctl call.
             */
            DEBUG("serial8250 device, attempting to open");
            if ((fd = open(name, O_RDWR | O_NONBLOCK | O_NOCTTY)) < 0) {
                DEBUG("Open failed, skipping");
                continue;
            }
#ifdef HAVE_STRUCT_SERIAL_STRUCT
            ioctl_result = ioctl(fd, TIOCGSERIAL, &serial_info);
#endif
            close(fd);
#ifdef HAVE_STRUCT_SERIAL_STRUCT
            if (ioctl_result != 0) {
                DEBUG("ioctl failed, skipping");
                continue;
            }
            if (serial_info.type == PORT_UNKNOWN) {
                DEBUG("Port type is unknown, skipping");
                continue;
            }
#endif
        }
        DEBUG_FMT("Found port %s", name);
        *list = list_append(*list, name);
        if (!*list) {
            SET_ERROR(ret, SP_ERR_MEM, "List append failed");
            break;
        }
    }
    closedir(dir);

    return ret;
}
#endif
