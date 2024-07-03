CC=gcc
CFLAGS=-g -Wall -Wextra
LDFLAGS=-lX11
MOD=example win draw prof
SRC=${MOD:%=%.c}
OBJ=${MOD:%=%.o}

example: $OBJ
	$CC $LDFLAGS -o $target $prereq

%.o: %.c
	$CC -c $CFLAGS -o $target $stem.c

<|$CC -MM $SRC
