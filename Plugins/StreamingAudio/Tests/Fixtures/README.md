# Fixtures

Original audio fixtures synthesised from tones and sweeps, and `manifest.json`, the oracle the MP3
tests compare against. `generate.py` regenerates both (it needs `ffmpeg` and `ffprobe` with
`libmp3lame`); frame counts in the manifest come from `ffprobe`, and header fields from the
generator's own parser.

| file | content |
|---|---|
| `mpeg1_cbr_stereo_44k.mp3`, `mpeg1_cbr_mono_44k.mp3`, `mpeg2_cbr_mono_22k.mp3`, `mpeg25_cbr_mono_8k.mp3`, `mpeg1_vbr_stereo_44k.mp3` | decodable Layer 3 streams: MPEG-1, MPEG-2 and MPEG-2.5, CBR and VBR, mono and stereo |
| `mpeg1_cbr_stereo_44k_corrupt.mp3` | the stereo stream with its middle frame zeroed |
| `layer2_stereo_44k.mp2` | a Layer 2 stream, which the decoder refuses |
| `pcm16_mono_16k.wav`, `pcm16_extensible_stereo_44k.wav`, `pcm24_stereo_44k.wav` | 16-bit WAV and 16-bit extensible PCM, which parse, and 24-bit WAV, which is refused |
| `layer1_header.bin`, `free_format_header.bin` | one frame header followed by zeros; header-classifier input, not audio streams |
