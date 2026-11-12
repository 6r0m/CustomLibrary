#include "StreamingAudioWav.h"
#include "Audio.h"

namespace
{
	constexpr uint16 WaveFormatPcm = 1;
}

EStreamingAudioResult FStreamingAudioWav::Read(TConstArrayView<uint8> FileBytes, FStreamingAudioFormat& OutFormat, TArray<uint8>& OutPcm)
{
	OutFormat = FStreamingAudioFormat();
	OutPcm.Reset();
	// The parser may rewrite a truncated data chunk's length in place, so it gets its own copy.
	const TArray<uint8> File(FileBytes);
	FWaveModInfo Info;
	if (File.IsEmpty() || !Info.ReadWaveInfo(File.GetData(), File.Num()))
	{
		return EStreamingAudioResult::InvalidInput;
	}
	if (*Info.pFormatTag != WaveFormatPcm || *Info.pBitsPerSample != 16)
	{
		return EStreamingAudioResult::UnsupportedFormat;
	}
	FStreamingAudioFormat Format;
	Format.SampleRate = static_cast<int32>(*Info.pSamplesPerSec);
	Format.NumChannels = *Info.pChannels;
	if (!Format.IsValid() || Info.SampleDataSize % Format.GetBytesPerFrame() != 0)
	{
		return EStreamingAudioResult::InvalidInput;
	}
	OutFormat = Format;
	OutPcm.Append(Info.SampleDataStart, Info.SampleDataSize);
	return EStreamingAudioResult::Ok;
}

EStreamingAudioResult FStreamingAudioWav::Write(const FStreamingAudioFormat& Format, TConstArrayView<uint8> Pcm, TArray<uint8>& OutFileBytes)
{
	OutFileBytes.Reset();
	if (!Format.IsValid() || Pcm.Num() % Format.GetBytesPerFrame() != 0)
	{
		return EStreamingAudioResult::InvalidInput;
	}
	SerializeWaveFile(OutFileBytes, Pcm.GetData(), Pcm.Num(), Format.NumChannels, Format.SampleRate);
	return EStreamingAudioResult::Ok;
}
