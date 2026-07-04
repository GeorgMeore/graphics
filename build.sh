#!/bin/sh -e

cflags="-I/opt/X11/include -Wall -Wextra -flto -fno-strict-aliasing -fwrapv"
ldflags="-L/opt/X11/lib -lX11"

usage() {
	echo "$0 [-h] [-d] [-OLEVEL]"
}

isnewer() {
	f=$1
	[ -f "$f" ] || return
	shift
	for n in "$@"; do
		! [ "$n" -nt "$f" ] || return
	done
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

isnewer win.o      win.c      ./*.h .cflags || cc -c $cflags -o win.o      win.c &
isnewer draw.o     draw.c     ./*.h .cflags || cc -c $cflags -o draw.o     draw.c &
isnewer prof.o     prof.c     ./*.h .cflags || cc -c $cflags -o prof.o     prof.c &
isnewer ntime.o    ntime.c    ./*.h .cflags || cc -c $cflags -o ntime.o    ntime.c &
isnewer panic.o    panic.c    ./*.h .cflags || cc -c $cflags -o panic.o    panic.c &
isnewer io.o       io.c       ./*.h .cflags || cc -c $cflags -o io.o       io.c &
isnewer image.o    image.c    ./*.h .cflags || cc -c $cflags -o image.o    image.c &
isnewer imagefmt.o imagefmt.c ./*.h .cflags || cc -c $cflags -o imagefmt.o imagefmt.c &
isnewer alloc.o    alloc.c    ./*.h .cflags || cc -c $cflags -o alloc.o    alloc.c &
isnewer math.o     math.c     ./*.h .cflags || cc -c $cflags -o math.o     math.c &
isnewer color.o    color.c    ./*.h .cflags || cc -c $cflags -o color.o    color.c &
isnewer poly.o     poly.c     ./*.h .cflags || cc -c $cflags -o poly.o     poly.c &
isnewer la.o       la.c       ./*.h .cflags || cc -c $cflags -o la.o       la.c &
isnewer font.o     font.c     ./*.h .cflags || cc -c $cflags -o font.o     font.c &
isnewer fontfmt.o  fontfmt.c  ./*.h .cflags || cc -c $cflags -o fontfmt.o  fontfmt.c &
wait

cd examples
isnewer 3d       3d.c       ../*.o ../*.h || cc $cflags -I.. -o 3d       3d.c       ../*.o $ldflags &
isnewer bezier   bezier.c   ../*.o ../*.h || cc $cflags -I.. -o bezier   bezier.c   ../*.o $ldflags &
isnewer circle   circle.c   ../*.o ../*.h || cc $cflags -I.. -o circle   circle.c   ../*.o $ldflags &
isnewer dragon   dragon.c   ../*.o ../*.h || cc $cflags -I.. -o dragon   dragon.c   ../*.o $ldflags &
isnewer io       io.c       ../*.o ../*.h || cc $cflags -I.. -o io       io.c       ../*.o $ldflags &
isnewer line     line.c     ../*.o ../*.h || cc $cflags -I.. -o line     line.c     ../*.o $ldflags &
isnewer nbody    nbody.c    ../*.o ../*.h || cc $cflags -I.. -o nbody    nbody.c    ../*.o $ldflags &
isnewer paint    paint.c    ../*.o ../*.h || cc $cflags -I.. -o paint    paint.c    ../*.o $ldflags &
isnewer poly     poly.c     ../*.o ../*.h || cc $cflags -I.. -o poly     poly.c     ../*.o $ldflags &
isnewer ppm      ppm.c      ../*.o ../*.h || cc $cflags -I.. -o ppm      ppm.c      ../*.o $ldflags &
isnewer sin      sin.c      ../*.o ../*.h || cc $cflags -I.. -o sin      sin.c      ../*.o $ldflags &
isnewer split    split.c    ../*.o ../*.h || cc $cflags -I.. -o split    split.c    ../*.o $ldflags &
isnewer triangle triangle.c ../*.o ../*.h || cc $cflags -I.. -o triangle triangle.c ../*.o $ldflags &
isnewer ttf      ttf.c      ../*.o ../*.h || cc $cflags -I.. -o ttf      ttf.c      ../*.o $ldflags &
#isnewer wav      wav.c      ../*.o ../*.h || cc $cflags -I.. -o wav      wav.c      ../*.o $ldflags &
isnewer y4m      y4m.c      ../*.o ../*.h || cc $cflags -I.. -o y4m      y4m.c      ../*.o $ldflags &
wait
