#include "mlib.h"
#include "utest.h"

TESTSUITE("macro library") {
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
	TESTCASE("DIVCEIL") {
		REQUIRE(DIVCEIL(5, 2) == 3);
		REQUIRE(DIVCEIL(5, 3) == 2);
		REQUIRE(DIVCEIL(5, -3) == -2);
		REQUIRE(DIVCEIL(-5, 3) == -2);
		REQUIRE(DIVCEIL(6, 3) == 2);
		REQUIRE(DIVCEIL(0, 3) == 0);
	}
	TESTCASE("DIVROUND") {
		REQUIRE(DIVROUND(5, 2) == 3);
		REQUIRE(DIVROUND(-5, 2) == -3);
		REQUIRE(DIVROUND(5, -2) == -3);
		REQUIRE(DIVROUND(-5, -2) == 3);
		REQUIRE(DIVROUND(6, 2) == 3);
		REQUIRE(DIVROUND(-6, 2) == -3);
		REQUIRE(DIVROUND(-6, -2) == 3);
		REQUIRE(DIVROUND(7, 3) == 2);
	}
	/* TODO: check more */
}
