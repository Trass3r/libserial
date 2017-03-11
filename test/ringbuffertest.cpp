#include <ringbufferbase.h>

#define CATCH_CONFIG_FAST_COMPILE
#include "catch.hpp"

#include <cstring>

TEST_CASE("buffer setup", "[ringbuffer]")
{
	const size_t size = 64 * 1024;

	RingBufferBase buffer(size);
	auto buf = buffer.base;
	REQUIRE(buf);

	static const char zero[2*size] = {};
	REQUIRE(memcmp(buf, zero, 2*size) == 0);

	memcpy(buf + size - 4, "12345678", 8);
	REQUIRE(memcmp(buf + size - 4, "1234", 4) == 0);
	REQUIRE(memcmp(buf + size, "5678", 4) == 0);
	REQUIRE(memcmp(buf, "5678", 4) == 0);
	REQUIRE(memcmp(buf + 2 * size - 4, "1234", 4) == 0);
}
