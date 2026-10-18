/* SPDX-License-Identifier: MIT */
/*
 * spukit demo: batch jobs spread over several SPEs, verified against the PPE.
 *
 *  1. Starts WORKERS persistent workers (one SPE each) running spu/mix.c.
 *  2. Splits a 4 MB buffer into slices, posts one job per worker per round,
 *     waits for all of them and compares every word with the PPE result and
 *     the checksums.
 *  3. Compresses a buffer with the LZ worker that ships with spukit
 *     (spukit_lzs), decompresses it on the PPE and compares.
 *
 * Output goes to the TTY (printf) and to /dev_hdd0/tmp/spukit-demo.log.
 * Last line: "RESULT spukit-demo: OK" or "FAIL".
 */
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/process.h>
#include <sys/systime.h>

#include <spukit/spukit.h>
#include <spukit/lzstream.h>

#include "job.h"
#include "spu_lz_bin.h"
#include "spu_mix_bin.h"

SYS_PROCESS_PARAM(1001, 0x100000);

#define WORKERS         4
#define ROUNDS          2
#define WORDS_PER_JOB   (128 * 1024)                       /* 512 KB */
#define TOTAL_WORDS     (WORKERS * ROUNDS * WORDS_PER_JOB) /* 4 MB */
#define LZ_BYTES        (1u << 20)

static FILE *logf;

static void say(const char *fmt, ...)
{
	char line[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(line, sizeof line, fmt, ap);
	va_end(ap);
	printf("%s\n", line);
	fflush(stdout);
	if (logf) {
		fprintf(logf, "%s\n", line);
		fflush(logf);
	}
}

struct lz_out { uint8_t *buf; uint32_t len; };

static int lz_sink(void *ctx, const void *data, uint32_t len)
{
	struct lz_out *o = ctx;
	memcpy(o->buf + o->len, data, len);
	o->len += len;
	return 0;
}

static int test_mix(void)
{
	uint32_t *src = memalign(128, TOTAL_WORDS * 4), *dst = memalign(128, TOTAL_WORDS * 4);
	if (!src || !dst)
		return say("no memory"), 0;
	uint32_t x = 0x12345678u;
	for (uint32_t i = 0; i < TOTAL_WORDS; i++) {
		x ^= x << 13; x ^= x >> 17; x ^= x << 5;
		src[i] = x;
	}
	memset(dst, 0, TOTAL_WORDS * 4);

	spukit_worker *w[WORKERS];
	for (int i = 0; i < WORKERS; i++) {
		w[i] = spukit_worker_start(spu_mix_bin, "mix", (uint64_t)i);
		if (!w[i])
			return say("spukit_worker_start(%d) failed: %d", i, spukit_last_error()), 0;
	}

	uint64_t t0 = sysGetSystemTime();
	uint32_t seq[WORKERS][ROUNDS];
	for (int r = 0; r < ROUNDS; r++)
		for (int i = 0; i < WORKERS; i++) {
			uint32_t first = (r * WORKERS + i) * WORDS_PER_JOB;
			struct demo_mix_job j = { (uintptr_t)(src + first), (uintptr_t)(dst + first), WORDS_PER_JOB, 0 };
			seq[i][r] = spukit_post(w[i], DEMO_MIX, &j, sizeof j);
		}

	int ok = 1;
	uint32_t spu_ticks = 0;
	for (int r = 0; r < ROUNDS; r++)
		for (int i = 0; i < WORKERS; i++) {
			const spukit_result *res = spukit_wait(w[i], seq[i][r]);
			uint32_t first = (r * WORKERS + i) * WORDS_PER_JOB, sum = 0;
			for (uint32_t k = 0; k < WORDS_PER_JOB; k++)
				sum += demo_mix_ref(src[first + k]);
			uint32_t got = ((const struct demo_mix_result *)res->data)->checksum;
			if (res->error != 0 || got != sum) {
				say("worker %d round %d: error=%d checksum 0x%08x (expected 0x%08x)", i, r, res->error, got, sum);
				ok = 0;
			}
			spu_ticks += res->ticks;
		}
	uint64_t us = sysGetSystemTime() - t0;

	uint32_t bad = 0;
	for (uint32_t i = 0; i < TOTAL_WORDS; i++)
		if (dst[i] != demo_mix_ref(src[i]))
			bad++;
	if (bad) {
		say("%u words differ from the PPE reference", bad);
		ok = 0;
	}
	say("mix: %d workers x %d jobs, %u MB, %u us wall, %u SPU ticks (%u Hz); mismatches %u",
	    WORKERS, ROUNDS, (unsigned)(TOTAL_WORDS * 4 >> 20), (unsigned)us, (unsigned)spu_ticks,
	    (unsigned)spukit_tick_hz(), bad);

	for (int i = 0; i < WORKERS; i++) {
		spukit_stats st;
		spukit_get_stats(w[i], &st);
		say("  worker %d: %u posted, %u doorbells, %u wakeups", i, st.posted, st.doorbells, st.wakeups);
		if (spukit_worker_stop(w[i]) != 0)
			ok = 0;
	}
	free(src);
	free(dst);
	return ok;
}

static int test_lz(void)
{
	uint8_t *src = memalign(128, LZ_BYTES);
	uint8_t *back = memalign(128, LZ_BYTES);
	struct lz_out o = { memalign(128, LZ_BYTES + LZ_BYTES / 200 + 4096), 0 };
	if (!src || !back || !o.buf)
		return say("no memory"), 0;
	uint32_t x = 1;
	for (uint32_t i = 0; i < LZ_BYTES; i++) {   /* compressible: runs and repeats */
		x = x * 1664525u + 1013904223u;
		src[i] = (i & 0x400) ? (uint8_t)(i >> 3) : (uint8_t)(x >> 28);
	}

	spukit_lzs *z = spukit_lzs_open(spu_lz_bin);
	if (!z)
		return say("spukit_lzs_open failed"), 0;
	spukit_lzs_stats st;
	int ok = spukit_lzs_run(z, src, LZ_BYTES, lz_sink, &o, &st) == 0;
	say("lz: %u -> %u bytes, %u blocks (%u on SPU, %u on PPE), %u us wall",
	    st.in_bytes, st.out_bytes, st.blocks, st.spu_blocks, st.ppe_blocks, (unsigned)st.wall_us);

	/* Decode block by block on the PPE and compare. */
	const uint8_t *p = o.buf;
	uint32_t done = 0;
	while (ok && done < LZ_BYTES) {
		uint32_t n = LZ_BYTES - done < SPUKIT_LZ_BLOCK ? LZ_BYTES - done : SPUKIT_LZ_BLOCK;
		uint32_t clen = spukit_lz_frame_len(p);
		int stored = spukit_lz_frame_stored(p);
		p += 4;
		if (stored)
			memcpy(back + done, p, n);
		else if (!spukit_lz_decompress(p, clen, back + done, n)) {
			say("block at %u does not decode", done);
			ok = 0;
		}
		p += stored ? n : clen;
		done += n;
	}
	if (ok && memcmp(src, back, LZ_BYTES) != 0) {
		say("decoded data differs from the original");
		ok = 0;
	}
	spukit_lzs_close(z);
	free(src); free(back); free(o.buf);
	return ok;
}

int main(void)
{
	logf = fopen("/dev_hdd0/tmp/spukit-demo.log", "w");
	say("spukit demo");
	int ok = spukit_init(6) == SPUKIT_OK;
	if (!ok)
		say("spukit_init failed");
	ok = ok && test_mix();
	ok = ok && test_lz();
	say("RESULT spukit-demo: %s", ok ? "OK" : "FAIL");
	if (logf)
		fclose(logf);
	return ok ? 0 : 1;
}
