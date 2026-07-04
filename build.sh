#!/bin/sh -e

osarch=macos_arm64
cflags="-I/opt/X11/include -I$PWD/platform/$osarch -I$PWD -Wall -Wextra -flto -fno-strict-aliasing -fwrapv"
ldflags="-L/opt/X11/lib -lX11"

usage() {
	echo "$0 [-h] [-d] [-OLEVEL]"
}

for a in "$@"; do
	case $a in
	-h)  usage; exit ;;
	-d)  cflags="$cflags -g -fsanitize=undefined,address" ;;
	-O*) cflags="$cflags $a" ;;
	*)   usage; exit 1 ;;
	esac
done

[ -e .cflags ] && [ "$(cat .cflags)" = "$cflags" ] || {
	printf "%s\n" "$cflags" >.cflags
}

echo "Flags: $cflags"

nonstale() {
	f=$1
	[ -f "$f" ] || return
	shift
	for n in "$@"; do
		! [ "$n" -nt "$f" ] || return
	done
}

nonstale win.o      win.c      ./*.h .cflags || cc -c $cflags -o win.o      win.c &
nonstale draw.o     draw.c     ./*.h .cflags || cc -c $cflags -o draw.o     draw.c &
nonstale prof.o     prof.c     ./*.h .cflags || cc -c $cflags -o prof.o     prof.c &
nonstale ntime.o    ntime.c    ./*.h .cflags || cc -c $cflags -o ntime.o    ntime.c &
nonstale panic.o    panic.c    ./*.h .cflags || cc -c $cflags -o panic.o    panic.c &
nonstale io.o       io.c       ./*.h .cflags || cc -c $cflags -o io.o       io.c &
nonstale image.o    image.c    ./*.h .cflags || cc -c $cflags -o image.o    image.c &
nonstale imagefmt.o imagefmt.c ./*.h .cflags || cc -c $cflags -o imagefmt.o imagefmt.c &
nonstale alloc.o    alloc.c    ./*.h .cflags || cc -c $cflags -o alloc.o    alloc.c &
nonstale math.o     math.c     ./*.h .cflags || cc -c $cflags -o math.o     math.c &
nonstale color.o    color.c    ./*.h .cflags || cc -c $cflags -o color.o    color.c &
nonstale poly.o     poly.c     ./*.h .cflags || cc -c $cflags -o poly.o     poly.c &
nonstale la.o       la.c       ./*.h .cflags || cc -c $cflags -o la.o       la.c &
nonstale font.o     font.c     ./*.h .cflags || cc -c $cflags -o font.o     font.c &
nonstale fontfmt.o  fontfmt.c  ./*.h .cflags || cc -c $cflags -o fontfmt.o  fontfmt.c &
nonstale jump.o platform/$osarch/jump.s .cflags || cc -c $cflags -o jump.o platform/$osarch/jump.s &
wait

cd examples
nonstale 3d       3d.c       ../*.o || cc $cflags -o 3d       3d.c       ../*.o $ldflags &
nonstale bezier   bezier.c   ../*.o || cc $cflags -o bezier   bezier.c   ../*.o $ldflags &
nonstale circle   circle.c   ../*.o || cc $cflags -o circle   circle.c   ../*.o $ldflags &
nonstale dragon   dragon.c   ../*.o || cc $cflags -o dragon   dragon.c   ../*.o $ldflags &
nonstale io       io.c       ../*.o || cc $cflags -o io       io.c       ../*.o $ldflags &
nonstale line     line.c     ../*.o || cc $cflags -o line     line.c     ../*.o $ldflags &
nonstale nbody    nbody.c    ../*.o || cc $cflags -o nbody    nbody.c    ../*.o $ldflags &
nonstale paint    paint.c    ../*.o || cc $cflags -o paint    paint.c    ../*.o $ldflags &
nonstale poly     poly.c     ../*.o || cc $cflags -o poly     poly.c     ../*.o $ldflags &
nonstale ppm      ppm.c      ../*.o || cc $cflags -o ppm      ppm.c      ../*.o $ldflags &
nonstale sin      sin.c      ../*.o || cc $cflags -o sin      sin.c      ../*.o $ldflags &
nonstale split    split.c    ../*.o || cc $cflags -o split    split.c    ../*.o $ldflags &
nonstale triangle triangle.c ../*.o || cc $cflags -o triangle triangle.c ../*.o $ldflags &
nonstale ttf      ttf.c      ../*.o || cc $cflags -o ttf      ttf.c      ../*.o $ldflags &
#nonstale wav      wav.c      ../*.o || cc $cflags -o wav      wav.c      ../*.o $ldflags &
nonstale y4m      y4m.c      ../*.o || cc $cflags -o y4m      y4m.c      ../*.o $ldflags &
wait
