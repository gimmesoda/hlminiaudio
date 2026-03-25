# TODO:

## Dependencies

- [x] convert lib `miniaudio` to submodule
- [x] use `libvorbis` + `ogg` instead of `stb_vorbis`
  - uses SSE / SSE2 / AVX for vectorized math (`stb_vorbis` is mostly scalar C)
  - optimized MDCT implementation
  - architecture-dependent optimizations
  - faster decoding via precomputed tables (trades memory for speed)
  - cache-friendly data structures and transform steps
  - tuned and specialized critical decoding stages
  - designed for maximum decoding performance, while `stb_vorbis` focuses on minimal code size and single-file simplicity

## Audio features

- support for audio formats:
  - [x] `wav`
  - [x] `mp3`
  - [x] `flac`
  - [x] `opus`
  - [x] `ogg`
  - [x] `aiff`
- [ ] better sound api implementation for heaps
- [ ] streaming support
- [ ] optimizations

## Memory management

- [ ] allocate sound objects through GC
- [ ] keep audio buffers alive while they are still referenced
- [ ] decide buffer lifetime strategy: refcounting vs full GC ownership
- [ ] add GC-safe handling for streamed / decoded audio data
- [ ] prevent premature buffer freeing when sounds share the same source

## Development

- [x] implement TODO's
- [ ] test cases

## Language / portability

- [x] translate codebase from `C++` to `C`, uhh so why not `C++`?:
  - simpler ABI and easier interop with other languages / FFI
  - easier embedding into existing C codebases and engines
  - lower runtime / language feature complexity
  - no exceptions / RTTI / templates overhead in build and maintenance
  - more predictable compilation across toolchains and platforms
  - better fit for minimal dependency and low-level systems code
  - easier integration with custom allocators, GC, and manual memory ownership rules
  - smaller binaries and simpler build pipeline
  - easier to audit, port, and debug in constrained environments
  - keeps public API straightforward and stable

## Build / CI

- [x] actions build via cmake in release mode for C++ `hdll` + `.lib`, all packed into one zip

## Docs

- [ ] add readme
