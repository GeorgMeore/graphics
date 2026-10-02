#!/bin/sh -e

usage() {
	echo "$0 [-h] [-d] [-oLEVEL]"
}

cd "${0%/*}" # in case we're called from other directory

case $(uname -sm) in
	'Darwin arm64')
		osarch=macos_arm64
		cflags="-DPAGE_SIZE=$(pagesize)"
		ldflags='-framework Cocoa -framework QuartzCore' ;;
	'Linux x86_64')
		osarch=linux_amd64
		ldflags='-lX11 -lpulse -lpulse-simple' ;;
	*)
		echo "error: unhandled os/arch"
		exit 1
esac

cppflags="-I$PWD/platform/$osarch -I$PWD"
cflags="$cflags -Wall -Wextra -flto -fno-strict-aliasing -fwrapv"

for a in "$@"; do
	case $a in
	-h)  usage; exit ;;
	-d)  cflags="$cflags -g -fsanitize=undefined,address" ;;
	-o*) cflags="$cflags -O${a#-o}" ;;
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
	# recompile the object if the source or any of the headers or cflags changed
	nonstale "$1" "$2" ./*.h ./platform/$osarch/*.h .cflags || cc -c $cppflags $cflags -o "$1" "$2"
}

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
case $osarch in
	linux_amd64) ccobj win.o platform/$osarch/win.c & ;;
	# TODO: we disable cppflags here because of filename conflicts,
	# this is a hack and I should probably find a better solution
	macos_arm64) cppflags= ccobj win.o platform/$osarch/win.m & ;;
esac
wait

ccexmpl() {
	# recompile the example if any of the object files changed
	nonstale "$1" "$1.c" ../*.o || cc $cppflags $cflags -o "$1" "$1.c" ../*.o $ldflags
}

cd examples
ccexmpl 3d &
ccexmpl bezier &
ccexmpl circle &
ccexmpl dragon &
ccexmpl io &
ccexmpl line &
ccexmpl nbody &
ccexmpl paint &
ccexmpl poly &
ccexmpl ppm &
ccexmpl sin &
ccexmpl split &
ccexmpl triangle &
ccexmpl ttf &
[ $osarch != linux_amd64 ] || ccexmpl wav &
ccexmpl y4m &
wait

# TODO: compile tests (TODO 2: write some decent tests)
