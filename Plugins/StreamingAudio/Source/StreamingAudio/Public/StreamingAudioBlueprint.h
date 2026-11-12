#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StreamingAudioFeeder.h"
#include "StreamingAudioMp3Decoder.h"
#include "StreamingAudioTypes.h"
#include "UObject/Object.h"
#include "StreamingAudioBlueprint.generated.h"

class USoundWaveProcedural;

/** Blueprint access to WAV parsing and one-shot queued sounds. */
UCLASS()
class STREAMINGAUDIO_API UStreamingAudioBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Parses a 16-bit PCM WAV file. */
	UFUNCTION(BlueprintCallable, Category = "Streaming Audio")
	static EStreamingAudioResult ReadWav(const TArray<uint8>& FileBytes, FStreamingAudioFormat& OutFormat, TArray<uint8>& OutPcm);

	/** Creates a wave holding all of Pcm, ready to play once. Game thread only. */
	UFUNCTION(BlueprintCallable, Category = "Streaming Audio")
	static EStreamingAudioResult CreateQueuedSound(const FStreamingAudioFormat& Format, const TArray<uint8>& Pcm, USoundWaveProcedural*& OutWave);
};

/** Blueprint access to FStreamingAudioFeeder. Game thread only. */
UCLASS(BlueprintType)
class STREAMINGAUDIO_API UStreamingAudioFeederObject : public UObject
{
	GENERATED_BODY()

public:
	/** Creates a feeder, or returns null with the reason in OutResult. */
	UFUNCTION(BlueprintCallable, Category = "Streaming Audio")
	static UStreamingAudioFeederObject* CreateFeeder(const FStreamingAudioFormat& Format, const FStreamingAudioFeederSettings& Settings, EStreamingAudioResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Streaming Audio")
	EStreamingAudioResult Append(const TArray<uint8>& Bytes);

	UFUNCTION(BlueprintCallable, Category = "Streaming Audio")
	EStreamingAudioResult Flush(int32& OutDroppedBytes);

	/** Starts a new stream on a new wave; play GetSoundWave() afterwards. */
	UFUNCTION(BlueprintCallable, Category = "Streaming Audio")
	EStreamingAudioResult Reset();

	UFUNCTION(BlueprintPure, Category = "Streaming Audio")
	USoundWaveProcedural* GetSoundWave() const;

	UFUNCTION(BlueprintPure, Category = "Streaming Audio")
	int32 GetUnderflowCount() const;

private:
	TUniquePtr<FStreamingAudioFeeder> Feeder;
};

/** Blueprint access to FStreamingAudioMp3Decoder. It belongs to the thread that created it. */
UCLASS(BlueprintType)
class STREAMINGAUDIO_API UStreamingAudioMp3DecoderObject : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Streaming Audio")
	static UStreamingAudioMp3DecoderObject* CreateMp3Decoder(const FStreamingAudioMp3Settings& Settings);

	UFUNCTION(BlueprintCallable, Category = "Streaming Audio")
	EStreamingAudioResult Append(const TArray<uint8>& Bytes, TArray<uint8>& OutPcm);

	UFUNCTION(BlueprintCallable, Category = "Streaming Audio")
	EStreamingAudioResult Finish(TArray<uint8>& OutPcm);

	UFUNCTION(BlueprintCallable, Category = "Streaming Audio")
	EStreamingAudioResult Reset();

	/** False until the first frame decodes. */
	UFUNCTION(BlueprintPure, Category = "Streaming Audio")
	bool GetFormat(FStreamingAudioFormat& OutFormat) const;

private:
	TUniquePtr<FStreamingAudioMp3Decoder> Decoder;
};
