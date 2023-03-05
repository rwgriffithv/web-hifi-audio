Web HiFi Audio (whfa)

Linux application to decode audio files and web streams, bypass mixing and resampling, and send PCM directly to sound cards (external DAC ideally)

using FFmpeg & ALSA
all "libav" references refer to the FFmpeg library system libav

near-term todo:

- TCP connection thread for receiving streamed file
- TCP connection thread for receiving and updating streaming state
- network API to allow for remote selection of local files
- Kotlin Android application to serve or select files w/ playback control
- parallel multipurpose processing of frames from queue (e.g. player & writer)

needs test:

- sound card device writing
- seeking while reading

dependencies:

- libavformat : file and stream parsing
- libavcodec : file and stream decoding
- libasound : sound card interfacing
