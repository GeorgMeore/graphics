CC=gcc
O=0 # no optimisations by default
D=0 # builds are not in debug mode by default
CDEBUGFLAGS=-g -fsanitize=undefined,address
CFLAGS=-I. -Wall -Wextra -O$O
LDFLAGS=-lX11 -lm
MOD=win draw prof ntime panic print image alloc
SRC=${MOD:%=%.c}
OBJ=${MOD:%=%.o}
PROGNAMES=example paint
PROGS=${PROGNAMES:%=prog/%}
UTESTNAMES=test_types test_mlib
UTESTS=${UTESTNAMES:%=test/%}

progs:V: $PROGS

tests:VQ: $UTESTS
	for test in $prereq; do
		$test
	done

test/%: test/%.c
	$CC $CFLAGS -o $target $prereq $LDFLAGS

prog/%: prog/%.c $OBJ
	[ "$D" != 0 ] && CFLAGS=$CFLAGS' '$CDEBUGFLAGS
	$CC $CFLAGS -o $target $prereq $LDFLAGS

%.o: %.c
	[ "$D" != 0 ] && CFLAGS=$CFLAGS' '$CDEBUGFLAGS
	$CC -c $CFLAGS -o $target $stem.c

clean:V:
	rm -rf $OBJ $PROGS $UTESTS

<|$CC -MM $SRC # generate dependencies
