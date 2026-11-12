#pragma once

#include "CoreMinimal.h"
#include "StreamingAudioTypes.h"

/** Splits PCM into chunks of whole frames. Any thread. */
struct STREAMINGAUDIO_API FStreamingAudioChunker
{
	/**
	 * Splits Pcm into consecutive chunks of ChunkSeconds each, rounded down to whole frames and at
	 * least one frame. The last chunk holds the rest and may be shorter. Returns InvalidInput for an
	 * invalid format, a duration that is not positive and finite, or PCM that is not whole frames.
	 * A duration longer than the input produces one chunk holding all frames.
	 */
	static EStreamingAudioResult Split(const FStreamingAudioFormat& Format, TConstArrayView<uint8> Pcm, double ChunkSeconds, TArray<TArray<uint8>>& OutChunks);
};
