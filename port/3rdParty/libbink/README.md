# libbink

The Bink 1 decoder of [libbink](https://github.com/raynorpat/libbink), which plays GTA2's movies.

- Source: https://github.com/raynorpat/libbink
- Commit: 73d884f45b0d6c360dd806a32e2c59766d7d34e1 (2026-11-16)
- Files: `src/*.c`, `src/*.h` and `LICENSE`, the decoder files only (not the encoder, players or tests), unmodified.
- Licence: LGPL 2.1 or later (`LICENSE`). The decoder is derived from FFmpeg's Bink decoder.

`port/CMakeLists.txt` builds it as a static C99 library.
