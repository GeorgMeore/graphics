CC=gcc
O=0 # no optimisations by default
CFLAGS=-I. -g -Wall -Wextra -O$O
LDFLAGS=-lX11 -lm
MOD=win draw prof util print
SRC=${MOD:%=%.c}
OBJ=${MOD:%=%.o}
PROGNAMES=example paint
PROGS=${PROGNAMES:%=prog/%}

all:VQ: $PROGS
	true

prog/%: prog/%.c $OBJ
	$CC $CFLAGS -o $target $prereq $LDFLAGS

%.o: %.c
	$CC -c $CFLAGS -o $target $stem.c

clean:V:
	rm -rf $OBJ $PROGS

<|$CC -MM $SRC
