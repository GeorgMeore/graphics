CC=gcc
O=0 # no optimisations by default
D=0 # builds are not in debug mode by default
CDEBUGFLAGS=-g -fsanitize=undefined,address
CFLAGS=-I. -Wall -Wextra -O$O
LDFLAGS=-lX11 -lm
MOD=win draw prof util print
SRC=${MOD:%=%.c}
OBJ=${MOD:%=%.o}
PROGNAMES=example paint
PROGS=${PROGNAMES:%=prog/%}

progs:VQ: $PROGS

prog/%: prog/%.c $OBJ
	[ "$D" != 0 ] && CFLAGS=$CFLAGS' '$CDEBUGFLAGS
	$CC $CFLAGS -o $target $prereq $LDFLAGS

%.o: %.c
	[ "$D" != 0 ] && CFLAGS=$CFLAGS' '$CDEBUGFLAGS
	$CC -c $CFLAGS -o $target $stem.c

clean:V:
	rm -rf $OBJ $PROGS

<|$CC -MM $SRC # generate dependencies
