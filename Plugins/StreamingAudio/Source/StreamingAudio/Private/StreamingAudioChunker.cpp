#include "StreamingAudioChunker.h"

EStreamingAudioResult FStreamingAudioChunker::Split(const FStreamingAudioFormat& Format, TConstArrayView<uint8> Pcm, double ChunkSeconds, TArray<TArray<uint8>>& OutChunks)
{
	OutChunks.Reset();
	if (!Format.IsValid() || !FMath::IsFinite(ChunkSeconds) || ChunkSeconds <= 0.0 || Pcm.Num() % Format.GetBytesPerFrame() != 0)
	{
		return EStreamingAudioResult::InvalidInput;
	}
	const int64 TotalFrames = Pcm.Num() / Format.GetBytesPerFrame();
	const int64 MaxFrames = FMath::Max<int64>(TotalFrames, 1);
	// Compare with the available duration before converting; a huge finite duration must not
	// overflow the float-to-int64 conversion. An oversized duration is one whole-input chunk.
	const double TotalSeconds = TotalFrames / static_cast<double>(Format.SampleRate);
	int64 FramesPerChunk = MaxFrames;
	if (ChunkSeconds < TotalSeconds)
	{
		FramesPerChunk = FMath::Clamp<int64>(static_cast<int64>(ChunkSeconds * Format.SampleRate), 1, MaxFrames);
	}
	const int64 BytesPerChunk = FramesPerChunk * Format.GetBytesPerFrame();
	for (int64 Offset = 0; Offset < Pcm.Num(); Offset += BytesPerChunk)
	{
		const int64 Size = FMath::Min<int64>(BytesPerChunk, Pcm.Num() - Offset);
		OutChunks.Emplace(Pcm.Slice(static_cast<int32>(Offset), static_cast<int32>(Size)));
	}
	return EStreamingAudioResult::Ok;
}
