Web HiFi Audio (whfa)

Linux application to decode audio files and web streams, bypass mixing and resampling, and send PCM directly to sound cards (external DAC ideally) through ALSA

near-term todo:

- libav context freeing and flushing (context.cpp)
- PCM format handling
- PCM file writing refactor
- ALSA sound card interfacing
- web API for local file support
- libav networked stream handling
- web API for remote file support
- better build scripts so subdirectories become libraries

needs test:

- multithreaded file reading, decoding, and basic controls
- PCM file writing
- EOF handling

dependencies:

- libavformat : file and stream parsing
- libavcodec : file and stream decoding
- alsa : sound card interfacing
