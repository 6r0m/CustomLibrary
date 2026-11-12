"""Regenerates the audio fixtures in this directory and their manifest.

Every fixture is synthesized here from tones and sweeps, so it is original and redistributable.
Needs ffmpeg and ffprobe (with libmp3lame) on PATH, or pass --ffmpeg-dir. Run with the engine's
bundled Python:

    <UE>/Engine/Binaries/ThirdParty/Python3/Win64/python.exe Tests/Fixtures/generate.py

The manifest is the tests' oracle: frame counts come from ffprobe, and the header fields from
this script's own header parser, which is independent of the plugin's.
"""

import argparse
import json
import shutil
import struct
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent

STEREO_TONES = "sin(2*PI*440*t)*0.4|sin(2*PI*660*t)*0.4"
MONO_TONE = "sin(2*PI*440*t)*0.4"
# Half a second of silence, then a rising sweep: a VBR encoder spends very different bitrates on each.
STEREO_SWEEP = "if(lt(t,0.5),0,sin(2*PI*(200+3000*t)*t)*0.5)|if(lt(t,0.5),0,sin(2*PI*(300+2500*t)*t)*0.5)"

ENCODED = [
    # name, expression, sample rate, seconds, codec arguments
    ("mpeg1_cbr_stereo_44k.mp3", STEREO_TONES, 44100, 2.0, ["-c:a", "libmp3lame", "-b:a", "128k"]),
    ("mpeg1_cbr_mono_44k.mp3", MONO_TONE, 44100, 1.0, ["-c:a", "libmp3lame", "-b:a", "64k"]),
    ("mpeg2_cbr_mono_22k.mp3", MONO_TONE, 22050, 1.0, ["-c:a", "libmp3lame", "-b:a", "32k"]),
    ("mpeg25_cbr_mono_8k.mp3", MONO_TONE, 8000, 1.0, ["-c:a", "libmp3lame", "-b:a", "16k"]),
    ("mpeg1_vbr_stereo_44k.mp3", STEREO_SWEEP, 44100, 2.0, ["-c:a", "libmp3lame", "-q:a", "4"]),
    ("layer2_stereo_44k.mp2", STEREO_TONES, 44100, 1.0, ["-c:a", "mp2", "-b:a", "192k"]),
]
WAVES = [
    ("pcm16_mono_16k.wav", MONO_TONE, 16000, 0.5, ["-c:a", "pcm_s16le"]),
    ("pcm24_stereo_44k.wav", STEREO_TONES, 44100, 0.25, ["-c:a", "pcm_s24le"]),
]
CORRUPTED = ("mpeg1_cbr_stereo_44k_corrupt.mp3", "mpeg1_cbr_stereo_44k.mp3")
# A 16-bit extensible-PCM WAV. Its subformat GUID is PCM; the engine parser redirects the tag.
EXTENSIBLE_WAVE = ("pcm16_extensible_stereo_44k.wav", 44100, 2, 100, 3)
# Parser fixtures: one header followed by zero bytes. They are not audio streams.
PARSER = [
    ("layer1_header.bin", bytes([0xFF, 0xFF, 0xC0, 0xC0]), 416),
    ("free_format_header.bin", bytes([0xFF, 0xFB, 0x00, 0xC0]), 420),
]

MPEG1_RATES = [44100, 48000, 32000]
BITRATES = {
    (1, 1): [0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448],
    (1, 2): [0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384],
    (1, 3): [0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320],
    (2, 1): [0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256],
    (2, 2): [0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160],
    (2, 3): [0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160],
}


def parse_header(data):
    """Returns the fields of the MPEG audio frame header at the start of data, or None."""
    if len(data) < 4 or data[0] != 0xFF or (data[1] & 0xE0) != 0xE0:
        return None
    version_bits, layer_bits = (data[1] >> 3) & 3, (data[1] >> 1) & 3
    bitrate_index, rate_index = data[2] >> 4, (data[2] >> 2) & 3
    if version_bits == 1 or layer_bits == 0 or bitrate_index == 15 or rate_index == 3:
        return None
    version = {3: "1", 2: "2", 0: "2.5"}[version_bits]
    layer = 4 - layer_bits
    rate = MPEG1_RATES[rate_index] >> {"1": 0, "2": 1, "2.5": 2}[version]
    kbps = BITRATES[(1 if version == "1" else 2, layer)][bitrate_index]
    padding = (data[2] >> 1) & 1
    samples = 384 if layer == 1 else (1152 if layer == 2 or version == "1" else 576)
    if kbps == 0:
        frame_bytes = 0
    elif layer == 1:
        frame_bytes = (12 * kbps * 1000 // rate + padding) * 4
    else:
        frame_bytes = samples // 8 * kbps * 1000 // rate + padding
    return {"version": version, "layer": layer, "bitrate_kbps": kbps, "sample_rate": rate,
            "channels": 1 if (data[3] >> 6) == 3 else 2, "samples_per_frame": samples, "frame_bytes": frame_bytes}


def walk_frames(data):
    """Frame offsets of a stream that starts on a frame and has no tags."""
    offsets, offset = [], 0
    while offset < len(data):
        header = parse_header(data[offset:offset + 4])
        if not header or header["frame_bytes"] == 0:
            raise SystemExit(f"no frame header at offset {offset}")
        offsets.append((offset, header))
        offset += header["frame_bytes"]
    return offsets


def run(tool, *arguments):
    return subprocess.run([str(tool), *arguments], check=True, capture_output=True, text=True).stdout


def encode(ffmpeg, name, expression, rate, seconds, codec):
    channels = expression.count("|") + 1
    run(ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi",
        "-i", f"aevalsrc='{expression}':s={rate}:d={seconds}:c={'stereo' if channels == 2 else 'mono'}",
        *codec, "-id3v2_version", "0", "-write_xing", "0", "-map_metadata", "-1", "-fflags", "+bitexact",
        "-flags:a", "+bitexact", str(HERE / name))


def write_extensible_wav(path, rate, channels, frames, seed):
    """Writes a deterministic 16-bit PCM WAV with a WAVE_FORMAT_EXTENSIBLE fmt chunk."""
    samples = [((index * 7 + seed * 131) % 30000) - 15000 for index in range(frames * channels)]
    data = struct.pack("<" + "h" * len(samples), *samples)
    subformat_pcm = bytes.fromhex("0100000000001000800000aa00389b71")
    fmt = struct.pack("<HHIIHH", 0xFFFE, channels, rate, rate * channels * 2, channels * 2, 16)
    fmt += struct.pack("<HHI", 22, 16, 3) + subformat_pcm
    body = b"fmt " + struct.pack("<I", len(fmt)) + fmt
    body += b"data" + struct.pack("<I", len(data)) + data
    path.write_bytes(b"RIFF" + struct.pack("<I", len(body) + 4) + b"WAVE" + body)


def probe_frames(ffprobe, path):
    output = run(ffprobe, "-v", "error", "-select_streams", "a:0", "-count_packets",
                 "-show_entries", "stream=nb_read_packets,sample_rate,channels,codec_name", "-of", "json", str(path))
    stream = json.loads(output)["streams"][0]
    return {"codec": stream["codec_name"], "frames": int(stream["nb_read_packets"]),
            "sample_rate": int(stream["sample_rate"]), "channels": int(stream["channels"])}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--ffmpeg-dir", help="directory holding ffmpeg and ffprobe (default: PATH)")
    args = parser.parse_args()
    ffmpeg = Path(args.ffmpeg_dir) / "ffmpeg" if args.ffmpeg_dir else shutil.which("ffmpeg")
    ffprobe = Path(args.ffmpeg_dir) / "ffprobe" if args.ffmpeg_dir else shutil.which("ffprobe")
    if not ffmpeg or not ffprobe:
        raise SystemExit("ffmpeg and ffprobe are required (PATH or --ffmpeg-dir)")

    manifest = {"audio": {}, "wave": {}, "parser": {}}
    for name, expression, rate, seconds, codec in ENCODED:
        encode(ffmpeg, name, expression, rate, seconds, codec)
        data = (HERE / name).read_bytes()
        frames = walk_frames(data)
        probed = probe_frames(ffprobe, HERE / name)
        first = frames[0][1]
        bitrates = sorted({header["bitrate_kbps"] for _, header in frames})
        if probed["frames"] != len(frames) or probed["sample_rate"] != first["sample_rate"] or probed["channels"] != first["channels"]:
            raise SystemExit(f"{name}: ffprobe {probed} disagrees with the header walk ({len(frames)} frames, {first})")
        if "-q:a" in codec and len(bitrates) < 2:
            raise SystemExit(f"{name}: a VBR fixture needs at least two bitrates, saw {bitrates}")
        manifest["audio"][name] = {
            "mpeg_version": first["version"], "layer": first["layer"], "vbr": "-q:a" in codec, "bitrates_kbps": bitrates,
            "frames": probed["frames"], "samples_per_frame": first["samples_per_frame"],
            "channels": probed["channels"], "sample_rate": probed["sample_rate"], "bytes": len(data),
        }

    corrupt_name, source_name = CORRUPTED
    data = bytearray((HERE / source_name).read_bytes())
    frames = walk_frames(bytes(data))
    middle, header = frames[len(frames) // 2]
    data[middle:middle + header["frame_bytes"]] = bytes(header["frame_bytes"])
    (HERE / corrupt_name).write_bytes(bytes(data))
    manifest["audio"][corrupt_name] = dict(manifest["audio"][source_name], corrupted_frame=len(frames) // 2,
                                           corrupted_bytes=[middle, header["frame_bytes"]])

    for name, expression, rate, seconds, codec in WAVES:
        channels = expression.count("|") + 1
        run(ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi",
            "-i", f"aevalsrc='{expression}':s={rate}:d={seconds}:c={'stereo' if channels == 2 else 'mono'}",
            *codec, "-map_metadata", "-1", "-fflags", "+bitexact", "-flags:a", "+bitexact", str(HERE / name))
        manifest["wave"][name] = {"sample_rate": rate, "channels": channels, "frames": round(rate * seconds),
                                  "bits_per_sample": 16 if codec[-1] == "pcm_s16le" else 24}

    name, rate, channels, frames, seed = EXTENSIBLE_WAVE
    write_extensible_wav(HERE / name, rate, channels, frames, seed)
    manifest["wave"][name] = {"sample_rate": rate, "channels": channels, "frames": frames,
                              "bits_per_sample": 16, "extensible": True}

    for name, header_bytes, size in PARSER:
        (HERE / name).write_bytes(header_bytes + bytes(size - len(header_bytes)))
        fields = parse_header(header_bytes)
        manifest["parser"][name] = {"label": "header bytes only, not an audio stream", "header_hex": header_bytes.hex(),
                                    "layer": fields["layer"], "bitrate_kbps": fields["bitrate_kbps"], "bytes": size}

    (HERE / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"wrote {sum(len(v) for v in manifest.values())} fixtures and manifest.json")


if __name__ == "__main__":
    main()
