#include "StreamingAudioFeeder.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/Package.h"
#include <atomic>

// Lives as long as the wave's underflow binding, which outlives the feeder's interest in the wave:
// the binding is never removed while the wave may render, so retirement is a flag, not an unbind.
struct FStreamingAudioFeeder::FUnderflowState
{
	std::atomic<int32> Count{0};
	std::atomic<bool> bRetired{false};
};

namespace
{
	USoundWaveProcedural* NewWave(const FStreamingAudioFormat& Format, float Duration)
	{
		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(GetTransientPackage());
		Wave->SetSampleRate(static_cast<uint32>(Format.SampleRate));
		Wave->NumChannels = Format.NumChannels;
		Wave->Duration = Duration;
		Wave->SoundGroup = SOUNDGROUP_Default;
		Wave->bLooping = false;
		return Wave;
	}
}

TUniquePtr<FStreamingAudioFeeder> FStreamingAudioFeeder::Create(const FStreamingAudioFormat& Format, const FStreamingAudioFeederSettings& Settings, EStreamingAudioResult& OutResult)
{
	if (!IsInGameThread())
	{
		OutResult = EStreamingAudioResult::WrongThread;
		return nullptr;
	}
	if (!Format.IsValid())
	{
		OutResult = EStreamingAudioResult::InvalidInput;
		return nullptr;
	}
	const double Frames = static_cast<double>(Settings.MaxQueuedSeconds) * Format.SampleRate;
	const double MaxBytes = FMath::FloorToDouble(Frames) * Format.GetBytesPerFrame();
	if (!FMath::IsFinite(Settings.MaxQueuedSeconds) || Frames < 1.0 || MaxBytes > MAX_int32)
	{
		OutResult = EStreamingAudioResult::InvalidSettings;
		return nullptr;
	}
	OutResult = EStreamingAudioResult::Ok;
	return TUniquePtr<FStreamingAudioFeeder>(new FStreamingAudioFeeder(Format, static_cast<int32>(MaxBytes)));
}

FStreamingAudioFeeder::FStreamingAudioFeeder(const FStreamingAudioFormat& InFormat, int32 InMaxQueuedBytes)
	: Format(InFormat)
	, MaxQueuedBytes(InMaxQueuedBytes)
{
	StartWave();
}

FStreamingAudioFeeder::~FStreamingAudioFeeder()
{
	FScopeLock Lock(&UnderflowStateLock);
	if (UnderflowState)
	{
		UnderflowState->bRetired = true;
	}
}

void FStreamingAudioFeeder::StartWave()
{
	Wave = NewWave(Format, INDEFINITELY_LOOPING_DURATION);
	TSharedPtr<FUnderflowState, ESPMode::ThreadSafe> NewState = MakeShared<FUnderflowState, ESPMode::ThreadSafe>();
	Wave->OnSoundWaveProceduralUnderflow.BindLambda([State = NewState](USoundWaveProcedural*, int32)
	{
		if (!State->bRetired)
		{
			++State->Count;
		}
	});
	FScopeLock Lock(&UnderflowStateLock);
	UnderflowState = NewState;
}

EStreamingAudioResult FStreamingAudioFeeder::Append(TConstArrayView<uint8> Bytes)
{
	if (!IsInGameThread())
	{
		return EStreamingAudioResult::WrongThread;
	}
	if (bFinished)
	{
		return EStreamingAudioResult::Finished;
	}
	if (Bytes.IsEmpty())
	{
		return EStreamingAudioResult::Ok;
	}
	const int32 FrameBytes = Format.GetBytesPerFrame();
	const int64 Total = static_cast<int64>(Remainder.Num()) + Bytes.Num();
	const int64 WholeBytes = Total - Total % FrameBytes;
	// Exact: nothing is ever queued to this wave from anywhere else, and it is never reset.
	if (static_cast<int64>(Wave->GetAvailableAudioByteCount()) + WholeBytes > MaxQueuedBytes)
	{
		return EStreamingAudioResult::CapExceeded;
	}
	Remainder.Append(Bytes.GetData(), Bytes.Num());
	if (WholeBytes > 0)
	{
		Wave->QueueAudio(Remainder.GetData(), static_cast<int32>(WholeBytes));
		Remainder.RemoveAt(0, static_cast<int32>(WholeBytes), EAllowShrinking::No);
	}
	return EStreamingAudioResult::Ok;
}

EStreamingAudioResult FStreamingAudioFeeder::Flush(int32& OutDroppedBytes)
{
	OutDroppedBytes = 0;
	if (!IsInGameThread())
	{
		return EStreamingAudioResult::WrongThread;
	}
	if (bFinished)
	{
		return EStreamingAudioResult::Finished;
	}
	OutDroppedBytes = Remainder.Num();
	Remainder.Reset();
	bFinished = true;
	return EStreamingAudioResult::Ok;
}

EStreamingAudioResult FStreamingAudioFeeder::Reset()
{
	if (!IsInGameThread())
	{
		return EStreamingAudioResult::WrongThread;
	}
	// Hand the wave off untouched: emptying its queue here would make this thread a second
	// consumer of a queue the audio renderer may be draining.
	{
		FScopeLock Lock(&UnderflowStateLock);
		UnderflowState->bRetired = true;
	}
	StartWave();
	Remainder.Reset();
	bFinished = false;
	return EStreamingAudioResult::Ok;
}

int32 FStreamingAudioFeeder::GetUnderflowCount() const
{
	TSharedPtr<FUnderflowState, ESPMode::ThreadSafe> State;
	{
		FScopeLock Lock(&UnderflowStateLock);
		State = UnderflowState;
	}
	return State ? State->Count.load() : 0;
}

EStreamingAudioResult FStreamingAudioFeeder::CreateQueuedSound(const FStreamingAudioFormat& Format, TConstArrayView<uint8> Pcm, USoundWaveProcedural*& OutWave)
{
	OutWave = nullptr;
	if (!IsInGameThread())
	{
		return EStreamingAudioResult::WrongThread;
	}
	if (!Format.IsValid() || Pcm.IsEmpty() || Pcm.Num() % Format.GetBytesPerFrame() != 0)
	{
		return EStreamingAudioResult::InvalidInput;
	}
	const float Seconds = static_cast<float>(Pcm.Num() / Format.GetBytesPerFrame()) / Format.SampleRate;
	USoundWaveProcedural* Wave = NewWave(Format, Seconds);
	Wave->QueueAudio(Pcm.GetData(), Pcm.Num());
	OutWave = Wave;
	return EStreamingAudioResult::Ok;
}

void FStreamingAudioFeeder::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Wave);
}

FString FStreamingAudioFeeder::GetReferencerName() const
{
	return TEXT("FStreamingAudioFeeder");
}
