CC=gcc
O=0 # no optimisations by default
D=1 # builds are not in debug mode by default
CDEBUGFLAGS=-g -fsanitize=undefined,address
CFLAGS=-I. -Wall -Wextra -O$O
LDFLAGS=-lX11
MOD=win draw prof ntime panic io image alloc math ppm
SRC=${MOD:%=%.c}
OBJ=${MOD:%=%.o}
PROGNAMES=split paint io bezier triangle circle line ppm sin
PROGS=${PROGNAMES:%=examples/%}
UTESTNAMES=test_types test_mlib
UTESTS=${UTESTNAMES:%=test/%}

examples:V: $PROGS

tests:VQ: $UTESTS
	for test in $prereq; do
		$test
	done

test/%: test/%.c
	$CC $CFLAGS -o $target $prereq $LDFLAGS

examples/%: examples/%.c $OBJ
	[ "$D" != 0 ] && CFLAGS=$CFLAGS' '$CDEBUGFLAGS
	$CC $CFLAGS -o $target $prereq $LDFLAGS

%.o: %.c
	[ "$D" != 0 ] && CFLAGS=$CFLAGS' '$CDEBUGFLAGS
	$CC -c $CFLAGS -o $target $stem.c

clean:V:
	rm -rf $OBJ $PROGS $UTESTS

<|$CC -MM $SRC # generate dependencies
