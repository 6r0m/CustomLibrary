#pragma once

#include "CoreMinimal.h"
#include "StreamingAudioTypes.h"

/** Reads and writes WAV files holding 16-bit PCM, through the engine's WAV support. Any thread. */
struct STREAMINGAUDIO_API FStreamingAudioWav
{
	/**
	 * Parses a WAV file. Returns InvalidInput for data that is not a WAV file, and
	 * UnsupportedFormat for anything other than integer PCM at 16 bits per sample (24-bit, float
	 * or compressed data). An extensible header is accepted when its subformat is PCM.
	 */
	static EStreamingAudioResult Read(TConstArrayView<uint8> FileBytes, FStreamingAudioFormat& OutFormat, TArray<uint8>& OutPcm);

	/** Writes PCM as a WAV file. Returns InvalidInput for an invalid format or PCM that is not whole frames. */
	static EStreamingAudioResult Write(const FStreamingAudioFormat& Format, TConstArrayView<uint8> Pcm, TArray<uint8>& OutFileBytes);
};
