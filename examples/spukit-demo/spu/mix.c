/* SPDX-License-Identifier: MIT */
/*
 * spukit demo: SPU program. One job = mix a slice of words with SIMD, DMA in
 * and out in 16 KB blocks.
 *
 * Local store budget (256 KB): 16 KB in + 16 KB out + spukit < 1 KB + code.
 */
#include <spukit/spu.h>
#include <sys/spu_thread.h>

#include "../job.h"

#define BLOCK 16384u

static vec_uint4 in_buf[BLOCK / 16] __attribute__((aligned(128)));
static vec_uint4 out_buf[BLOCK / 16] __attribute__((aligned(128)));

static int handler(void *ctx, const spukit_job *job, spukit_result *res)
{
	(void)ctx;
	if (job->kind != DEMO_MIX)
		return -2;
	const struct demo_mix_job *p = (const struct demo_mix_job *)job->data;
	const vec_uint4 add = spu_splats(DEMO_MIX_ADD);
	vec_uint4 sum = spu_splats(0u);
	uint32_t left = p->words * 4u, off = 0;

	while (left) {
		uint32_t n = left < BLOCK ? left : BLOCK;
		spukit_dma_get(in_buf, p->src_ea + off, n, 0);
		spukit_dma_wait(1u << 0);
		for (uint32_t k = 0; k < n / 16; k++) {
			vec_uint4 x = in_buf[k];
			vec_uint4 y = spu_xor(spu_rl(x, DEMO_MIX_ROT), spu_add(x, add));
			out_buf[k] = y;
			sum = spu_add(sum, y);
		}
		spukit_dma_put(out_buf, p->dst_ea + off, n, 0);
		spukit_dma_wait(1u << 0);
		off += n;
		left -= n;
	}
	struct demo_mix_result *r = (struct demo_mix_result *)res->data;
	r->checksum = spu_extract(sum, 0) + spu_extract(sum, 1) + spu_extract(sum, 2) + spu_extract(sum, 3);
	return 0;
}

int main(uint64_t ctrl_ea, uint64_t arg, uint64_t a2, uint64_t a3)
{
	(void)arg; (void)a2; (void)a3;
	spukit_run(ctrl_ea, handler, NULL);
	spu_thread_exit(0);
	return 0;
}
