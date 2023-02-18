Web HiFi Audio (whfa)

Linux application to decode audio files and web streams, bypass mixing and resampling, and send PCM directly to sound cards (external DAC ideally) through ALSA

near-term todo:

- queue blocking timeouts
- PCM format handling
- PCM file writing refactor
- libav networked stream handling
- ALSA sound card interfacing

needs test:

- multithreaded file reading & decoding
- PCM file writing
- EOF handling

dependencies:

- alsa : sound card interfacing
- libavformat : file and stream parsing
- libavcodec : file and stream decoding
