# hlminiaudio

> [!WARNING]
> **Project is still in development!**</br>
> This project is currently very early and not stable yet. Things may be broken, incomplete, or change at any time.</br>
> You can try the latest experimental version in the **nightly releases**, but expect bugs and rough edges.


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
- [ ] device enumeration api
- [ ] audio input / capture api
- [ ] microphone recording support
- [ ] duplex mode support (`playback` + `capture`)
- [ ] input/output device selection by id / name
- [ ] hotplug handling for audio devices
- expose device info:
  - [ ] backend name
  - [ ] device name
  - [ ] default sample rate
  - [ ] channel count
  - [ ] native sample format
  - [ ] is default device
- recording helpers:
  - [ ] start / stop / pause recording
  - [ ] capture into dynamic buffer
  - [ ] capture directly into file
  - [ ] user callback for live input processing
- [ ] resampling / channel conversion for capture streams
- [ ] low-latency playback / capture mode
- [ ] expose backend-specific controls where possible

## Effects / DSP

- [ ] basic DSP / effects API
- [ ] effect chain / processing graph via `ma_node_graph`
- [ ] gain / volume control
  - [ ] per-sound volume
  - [ ] per-bus volume
  - [ ] master / bus gain
- [ ] pan support
  - [ ] simple stereo pan via `ma_sound_set_pan()`
  - [ ] optional lower-level panner bindings
- [ ] delay / echo effect via `ma_delay_node`
- [ ] low-pass filter via `ma_lpf_node`
- [ ] high-pass filter via `ma_hpf_node`
- [ ] optional band-pass / biquad filter support
- [ ] wet / dry mix controls
- [ ] per-sound effect routing
- [ ] bus / master effects
- [ ] parallel routing via splitter node
- [ ] runtime parameter updates for supported effects
- [ ] custom DSP nodes for missing effects / advanced automation
- [ ] reverb support
  - [ ] evaluate `extras/nodes/ma_reverb_node`
  - [ ] document current limitations (`stereo-only`)
  - [ ] decide whether to ship as optional extra or custom integrated node
- [ ] decide implementation strategy:
  - [ ] use built-in `miniaudio` nodes where available
  - [ ] use `extras` nodes only as optional / experimental features
  - [ ] add custom DSP layer for unsupported or limited effects
  - [ ] keep public API backend-agnostic

## Memory management

- [ ] allocate sound objects through GC
- [ ] keep audio buffers alive while they are still referenced
- [ ] decide buffer lifetime strategy: refcounting vs full GC ownership
- [ ] add GC-safe handling for streamed / decoded audio data
- [ ] prevent premature buffer freeing when sounds share the same source
- [ ] GC-safe lifetime for capture / recording buffers
- [ ] shared ownership model for effect chains / DSP nodes
- [ ] zero-copy path where safe for streamed and captured audio

## Bindings / public API

- [ ] add bindings for playback device control
- [ ] add bindings for capture device control
- [ ] add bindings for device enumeration
- [ ] add bindings for recording api
- [ ] add bindings for stream callbacks
- [ ] add bindings for effect / DSP api
- [ ] add bindings for backend / device capability queries
- add high-level convenience API:
  - [ ] `listPlaybackDevices()`
  - [ ] `listCaptureDevices()`
  - [ ] `openPlaybackDevice()`
  - [ ] `openCaptureDevice()`
  - [ ] `startRecording()`
  - [ ] `stopRecording()`
  - [ ] `createEffectChain()`
- [ ] validate stable C ABI for all exported audio/device types
- [ ] document ownership rules in bindings

## Development

- [ ] test cases
- [ ] playback tests
- [ ] decode / stream tests
- [ ] device enumeration tests
- [ ] capture / recording tests
- [ ] duplex mode tests
- [ ] effect chain tests
- [ ] stress tests for buffer lifetime / GC interactions

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
- verify cross-platform audio backends:
  - [ ] `Windows` (`WASAPI`, fallback if needed)
  - [ ] `Linux` (`PulseAudio`, `ALSA`, optionally `JACK`)
  - [ ] `macOS` (`CoreAudio`)
  - [ ] `iOS`
  - [ ] `Android`
  - [ ] `Web` / `Emscripten` if feasible

## Build / CI

- [x] actions build via cmake in release mode for C++ `hdll` + `.lib`, all packed into one zip
- add CI builds for more platforms:
  - [x] `Windows`
  - [ ] `Linux`
  - [ ] `Linux arm64`
  - [ ] `macOS`
  - [ ] `macOS arm64`

## Docs

- [ ] add fully readme
- add examples:
  - [ ] basic playback
  - [ ] streaming playback
  - [ ] microphone capture
  - [ ] record to file
  - [ ] duplex input/output
  - [ ] effect chain setup
