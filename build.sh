#!/bin/sh -e

usage() {
	echo "$0 [-h] [-d] [-OLEVEL]"
}

case $(uname -sm) in
	'Darwin arm64')
		osarch=macos_arm64
		cflags=-I/opt/X11/include
		ldflags=-L/opt/X11/lib ;;
	'Linux x86_64')
		osarch=linux_amd64
		ldflags='-lpulse -lpulse-simple' ;;
	*)
		echo "error: unhandled os/arch"
		exit 1
esac

cflags="$cflags -I$PWD/platform/$osarch -I$PWD -Wall -Wextra -flto -fno-strict-aliasing -fwrapv"
ldflags="$ldflags -lX11"

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

ccobj() {
	# recompile the object if the source or any of the headers or cflags change
	nonstale "$1" "$2" ./*.h ./platform/$osarch/*.h .cflags || cc -c $cflags -o "$1" "$2"
}

ccobj win.o      win.c &
ccobj draw.o     draw.c &
ccobj prof.o     prof.c &
ccobj ntime.o    ntime.c &
ccobj panic.o    panic.c &
ccobj io.o       io.c &
ccobj image.o    image.c &
ccobj imagefmt.o imagefmt.c &
ccobj alloc.o    alloc.c &
ccobj math.o     math.c &
ccobj color.o    color.c &
ccobj poly.o     poly.c &
ccobj la.o       la.c &
ccobj font.o     font.c &
ccobj fontfmt.o  fontfmt.c &
ccobj jump.o     platform/$osarch/jump.s &
wait

ccexmpl() {
	# recompile the example if any of the object files changed
	nonstale "$1" "$2" ../*.o || cc $cflags -o "$1" "$2" ../*.o $ldflags
}

cd examples
ccexmpl 3d       3d.c &
ccexmpl bezier   bezier.c &
ccexmpl circle   circle.c &
ccexmpl dragon   dragon.c &
ccexmpl io       io.c &
ccexmpl line     line.c &
ccexmpl nbody    nbody.c &
ccexmpl paint    paint.c &
ccexmpl poly     poly.c &
ccexmpl ppm      ppm.c &
ccexmpl sin      sin.c &
ccexmpl split    split.c &
ccexmpl triangle triangle.c &
ccexmpl ttf      ttf.c &
[ $osarch != linux_amd64 ] || ccexmpl wav      wav.c &
ccexmpl y4m      y4m.c &
wait
