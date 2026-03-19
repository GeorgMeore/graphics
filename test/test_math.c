#include "types.h"
#include "math.h"
#include "utest.h"

TESTSUITE("math library") {
	TESTCASE("BOOL") {
		REQUIRE(BOOL(1) == 1);
		REQUIRE(BOOL(0) == 0);
		REQUIRE(BOOL(10) == 1);
		REQUIRE(BOOL(2) == 1);
		REQUIRE(BOOL(-1) == 1);
		REQUIRE(BOOL(-2) == 1);
	}
	TESTCASE("SIGN") {
		REQUIRE(SIGN(1) == 1);
		REQUIRE(SIGN(-1) == -1);
		REQUIRE(SIGN(0) == 0);
		REQUIRE(SIGN((unsigned)1) == 1);
		REQUIRE(SIGN((unsigned)-1) == 1);
		REQUIRE(SIGN((unsigned)0) == 0);
	}
	TESTCASE("DIVROUND") {
		REQUIRE(divround(5, 2) == 3);
		REQUIRE(divround(-5, 2) == -3);
		REQUIRE(divround(5, -2) == -3);
		REQUIRE(divround(-5, -2) == 3);
		REQUIRE(divround(6, 2) == 3);
		REQUIRE(divround(-6, 2) == -3);
		REQUIRE(divround(-6, -2) == 3);
		REQUIRE(divround(7, 3) == 2);
	}
	TESTCASE("lsb") {
		REQUIRE(lsb((unsigned)0b1)    == 0b1);
		REQUIRE(lsb((unsigned)0b10)   == 0b10);
		REQUIRE(lsb((unsigned)0b100)  == 0b100);
		REQUIRE(lsb((unsigned)0b101)  == 0b1);
		REQUIRE(lsb((unsigned)0b111)  == 0b1);
		REQUIRE(lsb((unsigned)0b1000) == 0b1000);
		REQUIRE(lsb((unsigned)0b1001) == 0b1);
		REQUIRE(lsb((unsigned)0b1111) == 0b1);
		REQUIRE(lsb((unsigned)0b1100) == 0b100);
	}
	/* TODO: check more */
}
