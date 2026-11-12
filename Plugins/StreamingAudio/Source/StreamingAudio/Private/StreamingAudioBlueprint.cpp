#include "StreamingAudioBlueprint.h"
#include "StreamingAudioWav.h"
#include "UObject/Package.h"

EStreamingAudioResult UStreamingAudioBlueprintLibrary::ReadWav(const TArray<uint8>& FileBytes, FStreamingAudioFormat& OutFormat, TArray<uint8>& OutPcm)
{
	return FStreamingAudioWav::Read(FileBytes, OutFormat, OutPcm);
}

EStreamingAudioResult UStreamingAudioBlueprintLibrary::CreateQueuedSound(const FStreamingAudioFormat& Format, const TArray<uint8>& Pcm, USoundWaveProcedural*& OutWave)
{
	return FStreamingAudioFeeder::CreateQueuedSound(Format, Pcm, OutWave);
}

UStreamingAudioFeederObject* UStreamingAudioFeederObject::CreateFeeder(const FStreamingAudioFormat& Format, const FStreamingAudioFeederSettings& Settings, EStreamingAudioResult& OutResult)
{
	TUniquePtr<FStreamingAudioFeeder> Feeder = FStreamingAudioFeeder::Create(Format, Settings, OutResult);
	if (!Feeder)
	{
		return nullptr;
	}
	UStreamingAudioFeederObject* Object = NewObject<UStreamingAudioFeederObject>(GetTransientPackage());
	Object->Feeder = MoveTemp(Feeder);
	return Object;
}

EStreamingAudioResult UStreamingAudioFeederObject::Append(const TArray<uint8>& Bytes)
{
	return Feeder ? Feeder->Append(Bytes) : EStreamingAudioResult::InvalidInput;
}

EStreamingAudioResult UStreamingAudioFeederObject::Flush(int32& OutDroppedBytes)
{
	OutDroppedBytes = 0;
	return Feeder ? Feeder->Flush(OutDroppedBytes) : EStreamingAudioResult::InvalidInput;
}

EStreamingAudioResult UStreamingAudioFeederObject::Reset()
{
	return Feeder ? Feeder->Reset() : EStreamingAudioResult::InvalidInput;
}

USoundWaveProcedural* UStreamingAudioFeederObject::GetSoundWave() const
{
	return Feeder ? Feeder->GetSoundWave() : nullptr;
}

int32 UStreamingAudioFeederObject::GetUnderflowCount() const
{
	return Feeder ? Feeder->GetUnderflowCount() : 0;
}

UStreamingAudioMp3DecoderObject* UStreamingAudioMp3DecoderObject::CreateMp3Decoder(const FStreamingAudioMp3Settings& Settings)
{
	UStreamingAudioMp3DecoderObject* Object = NewObject<UStreamingAudioMp3DecoderObject>(GetTransientPackage());
	Object->Decoder = MakeUnique<FStreamingAudioMp3Decoder>(Settings);
	return Object;
}

EStreamingAudioResult UStreamingAudioMp3DecoderObject::Append(const TArray<uint8>& Bytes, TArray<uint8>& OutPcm)
{
	if (!Decoder)
	{
		Decoder = MakeUnique<FStreamingAudioMp3Decoder>();
	}
	return Decoder->Append(Bytes, OutPcm);
}

EStreamingAudioResult UStreamingAudioMp3DecoderObject::Finish(TArray<uint8>& OutPcm)
{
	if (!Decoder)
	{
		Decoder = MakeUnique<FStreamingAudioMp3Decoder>();
	}
	return Decoder->Finish(OutPcm);
}

EStreamingAudioResult UStreamingAudioMp3DecoderObject::Reset()
{
	if (!Decoder)
	{
		Decoder = MakeUnique<FStreamingAudioMp3Decoder>();
	}
	return Decoder->Reset();
}

bool UStreamingAudioMp3DecoderObject::GetFormat(FStreamingAudioFormat& OutFormat) const
{
	OutFormat = Decoder ? Decoder->GetFormat() : FStreamingAudioFormat();
	return OutFormat.IsValid();
}
