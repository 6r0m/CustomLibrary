#pragma once

#include "CoreMinimal.h"
#include "StreamingAudioTypes.generated.h"

/** Result of a streaming-audio operation. A call that does not return Ok changes nothing, unless its documentation says otherwise. */
UENUM(BlueprintType)
enum class EStreamingAudioResult : uint8
{
	Ok,
	/** The input is malformed: an invalid format, data that is not whole frames, or a non-positive duration. */
	InvalidInput,
	/** The settings are out of range. */
	InvalidSettings,
	/** The call would exceed the configured memory cap. */
	CapExceeded,
	/** The call came from a thread other than the owning one. */
	WrongThread,
	/** The stream has ended. Reset starts a new one. */
	Finished,
	/** The stream failed earlier. Reset starts a new one. */
	Failed,
	/** The data uses an encoding this plugin does not decode. */
	UnsupportedFormat,
	/** The stream changed its sample rate or channel count. */
	FormatChanged,
};

/** Shape of interleaved signed 16-bit little-endian PCM. */
USTRUCT(BlueprintType)
struct STREAMINGAUDIO_API FStreamingAudioFormat
{
	GENERATED_BODY()

	/** Frames per second, 1 to 384000. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming Audio")
	int32 SampleRate = 0;

	/** Interleaved channels per frame, 1 to 8. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming Audio")
	int32 NumChannels = 0;

	static constexpr int32 BytesPerSample = 2;

	bool IsValid() const { return SampleRate > 0 && SampleRate <= 384000 && NumChannels >= 1 && NumChannels <= 8; }
	int32 GetBytesPerFrame() const { return NumChannels * BytesPerSample; }
	bool operator==(const FStreamingAudioFormat& Other) const { return SampleRate == Other.SampleRate && NumChannels == Other.NumChannels; }
	bool operator!=(const FStreamingAudioFormat& Other) const { return !(*this == Other); }
};
