CC=gcc
O=0 # no optimisations by default
CFLAGS=-g -Wall -Wextra -O$O
LDFLAGS=-lX11 -lm
MOD=example win draw prof util
SRC=${MOD:%=%.c}
OBJ=${MOD:%=%.o}

example: $OBJ
	$CC $LDFLAGS -o $target $prereq

%.o: %.c
	$CC -c $CFLAGS -o $target $stem.c

<|$CC -MM $SRC
