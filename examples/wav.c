#include "types.h"
#include "io.h"
#include "alloc.h"

#include <pulse/simple.h>
#include <pulse/error.h>

typedef struct {
	Arena mem;
	U16   nchan;
	U32   srate;
	U16   bps;
	U32   nframes;
	F32   *data;
} Audio;

static U64 readle(IOBuffer *b, U8 bytes)
{
	U64 n = 0;
	for (U8 i = 0, shift = 0; i < bytes; i++, shift += 8) {
		I c = bread(b);
		if (c == -1)
			return 0;
		n = n | (U64)c << shift;
	}
	return n;
}

static I64 readles(IOBuffer *b, U8 bytes)
{
	U64 n = readle(b, bytes);
	U64 sm = (U64)1 << (bytes*8 - 1);
	if (n & sm)
		n |= ~((sm >> 1) - 1); /* sign extension */
	return n;
}

static void skip(IOBuffer *b, U32 bytes)
{
	for (U32 i = 0; i < bytes; i++)
		bread(b);
}

static U32 strid(char s[4])
{
	return s[0]<<(0*8)|s[1]<<(1*8)|s[2]<<(2*8)|s[3]<<(3*8);
}

OK parsefmt(IOBuffer *b, Audio *a, U64 fmt)
{
	bseek(b, fmt);
	skip(b, 4); /* id */
	U32 size = readle(b, 4);
	if (size < 16)
		return 0;
	U16 format = readle(b, 2);
	if (format != 1)
		return 0;
	a->nchan = readle(b, 2);
	a->srate = readle(b, 4);
	/* TODO: use these fields for additional validation? */
	skip(b, 4+2); /* byte_rate, block_align */
	a->bps = readle(b, 2);
	return 1;
}

OK parsedata(IOBuffer *b, Audio *a, U64 data)
{
	if (a->bps != 16 && a->bps != 24 && a->bps != 32)
		return 0; /* TODO: support 8 bit pcm */
	bseek(b, data);
	skip(b, 4); /* id */
	U32 size = readle(b, 4);
	U32 framesize = a->bps * a->nchan / 8;
	if (size % framesize)
		return 0;
	a->nframes = size / framesize;
	a->data = aralloc(&a->mem, a->nframes * a->nchan * sizeof(F32));
	F32 max = 1 << (a->bps - 1);
	for (U32 i = 0; i < a->nframes; i++) {
		for (U16 ch = 0; ch < a->nchan; ch++) {
			F32 v = readles(b, a->bps/8);
			a->data[i*a->nchan + ch] = v / max;
		}
	}
	return 1;
}

Audio loadwav(const char *path)
{
	IOBuffer b = {0};
	if (!bopen(&b, path, 'r'))
		return (Audio){0};
	Audio a = {0};
	U32 riff = readle(&b, 4);
	if (riff != strid("RIFF"))
		return (Audio){0};
	U32 fsize = readle(&b, 4);
	U32 format = readle(&b, 4);
	if (format != strid("WAVE"))
		return (Audio){0};
	U64 fmt = 0, data = 0;
	while (b.pos < fsize + 8) {
		U64 start = b.pos;
		U32 id = readle(&b, 4);
		U32 size = readle(&b, 4);
		if (id == strid("fmt "))
			fmt = start;
		if (id == strid("data"))
			data = start;
		bseek(&b, b.pos + size + (size & 1));
	}
	if (!fmt || !data)
		return (Audio){0};
	OK ok = parsefmt(&b, &a, fmt) && parsedata(&b, &a, data);
	if (!ok || b.error) {
		arfree(&a.mem);
		return (Audio){0};
	}
	return a;
}

void playwav(Audio *a)
{
	pa_sample_spec ss = {
		.format = PA_SAMPLE_FLOAT32NE,
		.channels = a->nchan,
		.rate = a->srate,
	};
	pa_simple *s = pa_simple_new(0, "Fooapp", PA_STREAM_PLAYBACK, 0, "Music", &ss, 0, 0, 0);
	if (!s) {
		println("error: failed to start a PA session");
		return;
	}
	int err;
	if (pa_simple_write(s, a->data, a->nframes*sizeof(F32)*a->nchan, &err)) {
		println("error: failed to submit samples: ", OD(err), OS(pa_strerror(err)));
		goto exit;
	}
	if (pa_simple_drain(s, &err)) {
		println("error: failed to drain the buffer: ", OS(pa_strerror(err)));
		goto exit;
	}
exit:
	pa_simple_free(s);
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		println("usage: ", OS(argv[0]), " FILE");
		return 1;
	}
	Audio a = loadwav(argv[1]);
	if (!a.data) {
		println("error: parsing failed");
		return 1;
	}
	println("PCM:");
	println("\tchannels: ", OD(a.nchan));
	println("\tsample rate: ", OD(a.srate));
	println("\tbps: ", OD(a.bps));
	println("\tframe count: ", OD(a.nframes));
	playwav(&a);
	return 0;
}
