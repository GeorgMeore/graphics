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
		REQUIRE(sizeof(U64) == 8);
		REQUIRE(sizeof(I64) == 8);
	}
	TESTCASE("check sign") {
		REQUIRE(ISUNSIGNED(U8) == 1);
		REQUIRE(ISUNSIGNED(I8) == 0);
		REQUIRE(ISUNSIGNED(U16) == 1);
		REQUIRE(ISUNSIGNED(I16) == 0);
		REQUIRE(ISUNSIGNED(U32) == 1);
		REQUIRE(ISUNSIGNED(I32) == 0);
		REQUIRE(ISUNSIGNED(U64) == 1);
		REQUIRE(ISUNSIGNED(I64) == 0);
	}
	TESTCASE("check bits per U8") {
		REQUIRE((U8)(0xFF + 1) == (U8)0);
	}
}
