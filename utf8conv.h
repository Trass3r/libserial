/**
* (C) Copyright 2017 Trass3r
* Use, modification and distribution are subject to
* the Boost Software License, Version 1.0.
* (see http://www.boost.org/LICENSE_1_0.txt)
*/
#pragma once

#include <cassert>

#ifdef _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

//! a length of -1 denotes a 0-terminated string
inline std::string utf16to8(const wchar_t* str, uint32_t len = (uint32_t)-1)
{
	assert(len != 0);

	std::string result;
	int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, str, (int)len, nullptr, 0, nullptr, nullptr);

	assert(size > 0);
	if (size == 0)
		return result;

	result.resize(static_cast<size_t>(size));
	size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, str, (int)len, const_cast<char*>(result.data()), size, nullptr, nullptr);

	assert(size > 0);
	return result;
}

//! get system error string for given error code
inline std::string windowsErrorString(DWORD err)
{
	std::string	string;
	LPWSTR output = nullptr;

	if (DWORD len = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&output, 0, nullptr))
	{
		string = utf16to8(output, len - 2); // strip \r\n
		LocalFree(output);
	}

	return string;
}

//! get last system error string
inline std::string windowsErrorString()
{
	return windowsErrorString(GetLastError());
}

#endif
