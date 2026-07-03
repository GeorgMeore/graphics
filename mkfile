NPROC=16
CC=cc
O=0 # no optimisations by default
D=1 # builds are not in debug mode by default
CDEBUGFLAGS=-g -fsanitize=undefined,address
CFLAGS=-I/opt/X11/include -I. -Wall -Wextra -O$O -flto -fno-strict-aliasing -fwrapv
LDFLAGS=-L/opt/X11/lib -lX11 #-lpulse -lpulse-simple
MOD=win draw prof ntime panic io image imagefmt alloc math color poly la font fontfmt
SRC=${MOD:%=%.c}
OBJ=${MOD:%=%.o}
PROGNAMES=split paint io bezier triangle circle line ppm sin y4m nbody poly ttf dragon 3d #wav
PROGS=${PROGNAMES:%=examples/%}
UTESTNAMES=test_types test_math
UTESTS=${UTESTNAMES:%=tests/%}

examples:V: $PROGS

tests:VQ: $UTESTS
	for test in $prereq; do
		$test
	done

test/%: test/%.c $OBJ
	[ "$D" != 0 ] && CFLAGS=$CDEBUGFLAGS' '$CFLAGS
	$CC $CFLAGS -o $target $prereq $LDFLAGS

examples/%: examples/%.c $OBJ
	[ "$D" != 0 ] && CFLAGS=$CDEBUGFLAGS' '$CFLAGS
	$CC $CFLAGS -o $target $prereq $LDFLAGS

%.o: %.c mkfile
	[ "$D" != 0 ] && CFLAGS=$CDEBUGFLAGS' '$CFLAGS
	$CC -c $CFLAGS -o $target $stem.c

clean:V:
	rm -rf $OBJ $PROGS $UTESTS

<|$CC $CFLAGS -MM $SRC # generate dependencies
