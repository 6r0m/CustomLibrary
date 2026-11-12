#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "StreamingAudioTypes.h"
#include "UObject/GCObject.h"
#include "StreamingAudioFeeder.generated.h"

class USoundWaveProcedural;

USTRUCT(BlueprintType)
struct STREAMINGAUDIO_API FStreamingAudioFeederSettings
{
	GENERATED_BODY()

	/** Upper bound on audio queued to the current wave and not yet played, in seconds of the stream's format. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming Audio")
	float MaxQueuedSeconds = 10.0f;
};

/**
 * Feeds PCM that arrives in pieces of any size to a procedural sound wave, one whole frame at a
 * time.
 *
 * States: Open, then Finished after Flush. Reset returns to Open on a new wave. The feeder belongs
 * to the game thread: it is created there, and calls from other threads return WrongThread. The
 * caller plays GetSoundWave(), and after Reset plays the new wave. Audio already queued on a wave
 * the feeder has handed off keeps playing until the caller stops the component playing it; the
 * feeder never touches that wave again.
 */
class STREAMINGAUDIO_API FStreamingAudioFeeder : public FGCObject
{
public:
	/** Creates a feeder and its first wave. Returns null with WrongThread, InvalidInput or InvalidSettings. */
	static TUniquePtr<FStreamingAudioFeeder> Create(const FStreamingAudioFormat& Format, const FStreamingAudioFeederSettings& Settings, EStreamingAudioResult& OutResult);

	virtual ~FStreamingAudioFeeder() override;

	/** Queues every whole frame of the carried bytes plus Bytes, and carries the rest. CapExceeded leaves everything unchanged. */
	EStreamingAudioResult Append(TConstArrayView<uint8> Bytes);

	/** Ends the stream. OutDroppedBytes receives the partial frame that could not be queued. */
	EStreamingAudioResult Flush(int32& OutDroppedBytes);

	/**
	 * Starts a new stream on a new wave and returns to Open. This is also cancel. The previous wave
	 * is released without being modified.
	 */
	EStreamingAudioResult Reset();

	/** The wave the feeder currently queues to. */
	USoundWaveProcedural* GetSoundWave() const { return Wave; }

	const FStreamingAudioFormat& GetFormat() const { return Format; }
	int32 GetMaxQueuedBytes() const { return MaxQueuedBytes; }
	bool IsFinished() const { return bFinished; }

	/** Underflows the current wave reported since it was created. Thread-safe. */
	int32 GetUnderflowCount() const;

	/**
	 * Creates a wave holding all of Pcm, ready to play once. Game thread only. Returns
	 * WrongThread, or InvalidInput for an invalid format or PCM that is not whole frames.
	 */
	static EStreamingAudioResult CreateQueuedSound(const FStreamingAudioFormat& Format, TConstArrayView<uint8> Pcm, USoundWaveProcedural*& OutWave);

	//~ FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

private:
	struct FUnderflowState;

	FStreamingAudioFeeder(const FStreamingAudioFormat& InFormat, int32 InMaxQueuedBytes);
	void StartWave();

	FStreamingAudioFormat Format;
	int32 MaxQueuedBytes = 0;
	TObjectPtr<USoundWaveProcedural> Wave;
	// Guards the shared-pointer handoff between Reset and a reader on another thread.
	mutable FCriticalSection UnderflowStateLock;
	TSharedPtr<FUnderflowState, ESPMode::ThreadSafe> UnderflowState;
	TArray<uint8> Remainder;
	bool bFinished = false;
};
