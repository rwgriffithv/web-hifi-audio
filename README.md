Web HiFi Audio (whfa)

Linux application to decode audio files and web streams, bypass mixing and resampling, and send PCM directly to sound cards (external DAC ideally)

using FFmpeg & ALSA
all "libav" references refer to the FFmpeg library system libav

near-term todo:

- better build scripts so subdirectories become libraries
- web API for local file support
- libav networked stream handling
- web API for remote file support
- Kotlin Android application to serve or select files w/ playback control

needs test:

- multithreaded file reading, decoding, and basic controls
- EOF handling
- raw PCM & WAV file writing
- sound card device writing

dependencies:

- libavformat : file and stream parsing
- libavcodec : file and stream decoding
- libasound : sound card interfacing
