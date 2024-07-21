#include "types.h"
#include "utest.h"

TESTSUITE("integer type definitions")
{
	TESTCASE("check sizes") {
		REQUIRE(sizeof(u8) == 1);
		REQUIRE(sizeof(s8) == 1);
		REQUIRE(sizeof(u16) == 2);
		REQUIRE(sizeof(s16) == 2);
		REQUIRE(sizeof(u32) == 4);
		REQUIRE(sizeof(s32) == 4);
	}
	TESTCASE("check bits per u8") {
		REQUIRE((u8)(0xFF + 1) == (u8)0);
	}
}
