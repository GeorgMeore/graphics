CC=gcc
O=0 # no optimisations by default
CFLAGS=-g -Wall -Wextra -O$O
LDFLAGS=-lX11 -lm
MOD=win draw prof util
SRC=${MOD:%=%.c}
OBJ=${MOD:%=%.o}
PROGS=example paint

all:VQ: $PROGS
	true

%: %.c $OBJ
	$CC -o $target $prereq $LDFLAGS

%.o: %.c
	$CC -c $CFLAGS -o $target $stem.c

clean:V:
	rm -rf $OBJ $PROGS

<|$CC -MM $SRC
