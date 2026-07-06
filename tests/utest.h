/* NOTE: this is no ordinary header file, this is essentially
 * a framework for unit-testing, so we do all sorts of WEIRD stuff here */

/* NOTE: We don't want to introduce any collisions, so instead of
 * including a bunch of crap from c stdlib we just declare
 * the things we need ourselves */
int printf(const char *fmt, ...);
void exit(int status);

static void runsuite(void);
static char *suitename;

int main(void)
{
	printf("RUN: %s\n", suitename);
	runsuite();
	printf("OK: %s\n", suitename);
	return 0;
}

#define TESTSUITE(name)\
static char *suitename = name;\
void runsuite(void)

#define TESTCASE(cname)\
	printf("\tCase: %s: ", cname);\
	for (int i = 0; i < 1; printf("OK\n"), i++)

#define REQUIRE(v)\
	if (!(v)) {\
		printf("FAIL %s:%d    %s\n", __FILE__, __LINE__, #v);\
		exit(1);\
	} else {};
