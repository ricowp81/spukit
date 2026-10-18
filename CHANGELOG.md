# Changelog

## v0.1.0 - 2026-10-18

First tagged release. The code is the library as used by Salsa64.

- Persistent SPU workers with a job queue in main memory and a mailbox doorbell
  (`spukit_init`, `spukit_worker_start`, `spukit_post`, `spukit_wait`,
  `spukit_worker_stop`), statistics, SPU-side loop and DMA helpers
  (`spukit/spu.h`).
- LZ block compression (LZ4 block format) on the PPE, the host and an SPE
  (`spukit/lz.h`, `spukit/lzstream.h`, `spu/lz.c`).
- New: `examples/spukit-demo` (builds an installable `.pkg`), English README,
  `README.es.md`, this changelog.

The API may still change before 1.0.
