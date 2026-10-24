# spukit

Batch jobs on the PlayStation 3's SPEs, for homebrew built with
[PSL1GHT](https://github.com/ps3dev/PSL1GHT). MIT licensed.

spukit started inside [Salsa64](https://github.com/ricowp81/Salsa64), a Nintendo 64
emulator for the PS3, and knows nothing about any emulated system. Salsa64 is a
real-world example of using it (graphics and audio work queues, save-state
compression).

**Status: v0.1.0.** The API is in use and works on real hardware, but it may still
change before 1.0. Pin a version.

## Model

- A **worker** is a persistent SPU thread in its own group (it occupies one SPE
  while it lives) that sleeps on its inbound mailbox.
- The PPE **posts** jobs to a queue in main memory: a 128-byte descriptor (a `kind`
  and up to 112 bytes of parameters, normally addresses of buffers) and rings a
  **doorbell** (a mailbox write). Posting never waits for the SPU unless the
  8-slot queue is full.
- The SPU serves everything posted, in order, writes a 128-byte result (error code,
  duration in decrementer ticks, up to 112 bytes of its own) and then the number of
  the last finished job. It waits for all its DMA first, so what the job wrote to
  main memory is visible to the PPE as soon as the job is seen as done.
- The PPE polls (`spukit_done`) or waits (`spukit_wait`) when it needs the result.

It is meant for big jobs (a whole command list per frame, an audio buffer, a
filter), not fine-grained chatter.

Also included: LZ block compression (LZ4 block format, written from the public
specification) usable on the PPE, on the host and on an SPE (`spukit/lz.h`,
`spukit/lzstream.h`, `spu/lz.c`).

## Building

Requires the PSL1GHT toolchain. With `PSL1GHT` set:

```sh
make                  # -> build/libspukit.a (PPE side)
make OUT=/some/dir
```

The SPU side is header-only (`include/spukit/spu.h`). The LZ worker program
`spu/lz.c` is built like any SPU program (see the example's Makefile).

## Using it

Add `-I<spukit>/include` for both the PPE and the SPU code, and link
`libspukit.a` on the PPE **before** `-llv2`:

```
-lspukit -lrt -llv2 -lm
```

PPE:

```c
#include <spukit/spukit.h>
#include "spu_prog_bin.h"                 /* the SPU ELF, embedded with bin2s */

spukit_init(6);
spukit_worker *w = spukit_worker_start(spu_prog_bin, "my_job", arg);
uint32_t seq = spukit_post(w, MY_KIND, &params, sizeof params);
/* ... the PPE does something else ... */
const spukit_result *r = spukit_wait(w, seq);    /* r->error, r->ticks, r->data */
spukit_worker_stop(w);
```

SPU:

```c
#include <spukit/spu.h>
#include <sys/spu_thread.h>

static int handler(void *ctx, const spukit_job *job, spukit_result *res)
{
	/* DMA in, compute, DMA out (tags 0-30; tag 31 belongs to the loop) */
	return 0;                             /* goes to res->error */
}

int main(uint64_t ctrl_ea, uint64_t arg, uint64_t a2, uint64_t a3)
{
	spukit_run(ctrl_ea, handler, NULL);
	spu_thread_exit(0);
	return 0;
}
```

## Example

`examples/spukit-demo` starts four workers running a SIMD transformation, spreads
8 jobs over them (4 MB), and checks every output word and checksum against the PPE.
It then compresses 1 MB with the LZ worker, decodes it on the PPE and compares.
Results go to the TTY (and to `/dev_hdd0/tmp/spukit-demo.log`); the last line is
`RESULT spukit-demo: OK` or `FAIL`.

```sh
make -C examples/spukit-demo pkg     # build/spukit-demo.self and .pkg (APPID SPUK00001)
```

It builds the library first. The package installs from the XMB (Package Manager)
as "spukit demo". Release assets include a prebuilt `.pkg`. There is no on-screen
output (the program has no video): read the log file or the TTY.

## Documentation

The headers in `include/spukit/` are the API reference; their comments and
`README.es.md` (a longer description, with the API tables) are in Spanish.

Third-party material: see [THIRD_PARTY.md](THIRD_PARTY.md).

## Support

If spukit is useful to you, you can buy me a coffee at
[buymeacoffee.com/ricowp81](https://buymeacoffee.com/ricowp81). Thanks!

## License

MIT, see [LICENSE](LICENSE). Each source file carries
`SPDX-License-Identifier: MIT`. No GPL code is included.
