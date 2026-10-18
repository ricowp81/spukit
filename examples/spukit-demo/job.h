/* SPDX-License-Identifier: MIT */
/* Job format shared by the PPE and the SPU program of the spukit demo. */
#ifndef DEMO_JOB_H
#define DEMO_JOB_H

#include <stdint.h>

#define DEMO_MIX      1u        /* job kind: transform a slice of words */
#define DEMO_MIX_ROT  7
#define DEMO_MIX_ADD  0x9E3779B9u

/* Parameters of a DEMO_MIX job (<= SPUKIT_DATA_SIZE bytes). */
struct demo_mix_job {
	uint64_t src_ea;   /* 16-byte aligned */
	uint64_t dst_ea;   /* 16-byte aligned */
	uint32_t words;    /* multiple of 4 */
	uint32_t pad;
};

/* Result data of a DEMO_MIX job (<= SPUKIT_RESULT_SIZE bytes). */
struct demo_mix_result {
	uint32_t checksum; /* sum of the output words, mod 2^32 */
};

static inline uint32_t demo_mix_ref(uint32_t x)
{
	return ((x << DEMO_MIX_ROT) | (x >> (32 - DEMO_MIX_ROT))) ^ (x + DEMO_MIX_ADD);
}

#endif
