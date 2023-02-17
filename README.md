Web HiFi Audio (whfa)

Linux application to decode audio files and web streams, bypass mixing and resampling, and send PCM directly to sound cards (external DAC ideally) through ALSA

Currently working on efficient multithreaded file reading, decoding, and playback

dependencies:
 * alsa : playback control
 * libavformat : file and stream parsing
 * libavcodec : file and stream decoding
