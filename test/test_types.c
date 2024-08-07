#include "types.h"
#include "utest.h"

TESTSUITE("integer type definitions")
{
	TESTCASE("check sizes") {
		REQUIRE(sizeof(U8) == 1);
		REQUIRE(sizeof(I8) == 1);
		REQUIRE(sizeof(U16) == 2);
		REQUIRE(sizeof(I16) == 2);
		REQUIRE(sizeof(U32) == 4);
		REQUIRE(sizeof(I32) == 4);
	}
	TESTCASE("check bits per U8") {
		REQUIRE((U8)(0xFF + 1) == (U8)0);
	}
}
