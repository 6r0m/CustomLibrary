#include "StreamingAudioMp3Decoder.h"
#include "HAL/PlatformTLS.h"

THIRD_PARTY_INCLUDES_START
#define MINIMP3_ONLY_MP3
#define MINIMP3_IMPLEMENTATION
#include "ThirdParty/minimp3/minimp3.h"
#undef MINIMP3_IMPLEMENTATION
#undef MINIMP3_ONLY_MP3
THIRD_PARTY_INCLUDES_END

static_assert(PLATFORM_LITTLE_ENDIAN, "Decoded samples are copied as little-endian bytes.");

struct FStreamingAudioMp3Decoder::FDecoderState
{
	mp3dec_t Decoder;
	mp3d_sample_t Samples[MINIMP3_MAX_SAMPLES_PER_FRAME];
};

namespace
{
	constexpr int32 Mpeg1Layer3Kbps[15] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320};
	constexpr int32 Mpeg2Layer3Kbps[15] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160};
	constexpr int32 Mpeg1SampleRates[3] = {44100, 48000, 32000};
}

EStreamingAudioResult FStreamingAudioMp3Decoder::ClassifyHeader(TConstArrayView<uint8> HeaderBytes, FStreamingAudioMp3Header& OutHeader)
{
	OutHeader = FStreamingAudioMp3Header();
	if (HeaderBytes.Num() < 4)
	{
		return EStreamingAudioResult::InvalidInput;
	}
	const uint8 B1 = HeaderBytes[1];
	const uint8 B2 = HeaderBytes[2];
	const uint8 B3 = HeaderBytes[3];
	const int32 VersionBits = (B1 >> 3) & 3;
	const int32 LayerBits = (B1 >> 1) & 3;
	const int32 BitrateIndex = B2 >> 4;
	const int32 RateIndex = (B2 >> 2) & 3;
	if (HeaderBytes[0] != 0xFF || (B1 & 0xE0) != 0xE0 || VersionBits == 1 || LayerBits == 0 || BitrateIndex == 15 || RateIndex == 3)
	{
		return EStreamingAudioResult::InvalidInput;
	}
	const bool bMpeg1 = VersionBits == 3;
	OutHeader.MpegVersionTimesTen = bMpeg1 ? 10 : (VersionBits == 2 ? 20 : 25);
	OutHeader.Layer = 4 - LayerBits;
	OutHeader.SampleRate = Mpeg1SampleRates[RateIndex] >> (bMpeg1 ? 0 : (VersionBits == 2 ? 1 : 2));
	OutHeader.NumChannels = (B3 >> 6) == 3 ? 1 : 2;
	if (OutHeader.Layer != 3)
	{
		OutHeader.SamplesPerFrame = OutHeader.Layer == 1 ? 384 : 1152;
		return EStreamingAudioResult::UnsupportedFormat;
	}
	OutHeader.SamplesPerFrame = bMpeg1 ? 1152 : 576;
	OutHeader.BitrateKbps = bMpeg1 ? Mpeg1Layer3Kbps[BitrateIndex] : Mpeg2Layer3Kbps[BitrateIndex];
	if (BitrateIndex == 0)
	{
		return EStreamingAudioResult::UnsupportedFormat;
	}
	const int32 Padding = (B2 >> 1) & 1;
	OutHeader.FrameBytes = OutHeader.SamplesPerFrame / 8 * OutHeader.BitrateKbps * 1000 / OutHeader.SampleRate + Padding;
	return EStreamingAudioResult::Ok;
}

FStreamingAudioMp3Decoder::FStreamingAudioMp3Decoder(const FStreamingAudioMp3Settings& InSettings)
	: Settings(InSettings)
	, Decoder(MakeUnique<FDecoderState>())
	, OwnerThreadId(FPlatformTLS::GetCurrentThreadId())
	, bValidSettings(InSettings.SyncWindowBytes >= DecodeMarginBytes && InSettings.MaxBufferedBytes >= InSettings.SyncWindowBytes)
{
	mp3dec_init(&Decoder->Decoder);
}

FStreamingAudioMp3Decoder::~FStreamingAudioMp3Decoder() = default;

EStreamingAudioResult FStreamingAudioMp3Decoder::CheckCaller() const
{
	if (FPlatformTLS::GetCurrentThreadId() != OwnerThreadId)
	{
		return EStreamingAudioResult::WrongThread;
	}
	return bValidSettings ? EStreamingAudioResult::Ok : EStreamingAudioResult::InvalidSettings;
}

EStreamingAudioResult FStreamingAudioMp3Decoder::Append(TConstArrayView<uint8> Bytes, TArray<uint8>& OutPcm)
{
	OutPcm.Reset();
	const EStreamingAudioResult Caller = CheckCaller();
	if (Caller != EStreamingAudioResult::Ok)
	{
		return Caller;
	}
	if (State == EStreamingAudioMp3State::Failed)
	{
		return EStreamingAudioResult::Failed;
	}
	if (State == EStreamingAudioMp3State::Finished)
	{
		return EStreamingAudioResult::Finished;
	}
	if (static_cast<int64>(Buffer.Num()) + Bytes.Num() > Settings.MaxBufferedBytes)
	{
		return EStreamingAudioResult::CapExceeded;
	}
	Buffer.Append(Bytes.GetData(), Bytes.Num());
	bSyncWindowReached |= Buffer.Num() >= Settings.SyncWindowBytes;
	if (!bSyncWindowReached)
	{
		return EStreamingAudioResult::Ok;
	}
	return DecodeHeld(false, OutPcm);
}

EStreamingAudioResult FStreamingAudioMp3Decoder::Finish(TArray<uint8>& OutPcm)
{
	OutPcm.Reset();
	const EStreamingAudioResult Caller = CheckCaller();
	if (Caller != EStreamingAudioResult::Ok)
	{
		return Caller;
	}
	if (State == EStreamingAudioMp3State::Failed)
	{
		return EStreamingAudioResult::Failed;
	}
	if (State == EStreamingAudioMp3State::Finished)
	{
		return EStreamingAudioResult::Finished;
	}
	const EStreamingAudioResult Result = DecodeHeld(true, OutPcm);
	if (Result == EStreamingAudioResult::Ok)
	{
		State = EStreamingAudioMp3State::Finished;
	}
	return Result;
}

EStreamingAudioResult FStreamingAudioMp3Decoder::Reset()
{
	const EStreamingAudioResult Caller = CheckCaller();
	if (Caller != EStreamingAudioResult::Ok)
	{
		return Caller;
	}
	mp3dec_init(&Decoder->Decoder);
	Buffer.Reset();
	Format = FStreamingAudioFormat();
	State = EStreamingAudioMp3State::Buffering;
	SkippedBytes = 0;
	TruncatedBytes = 0;
	bSyncWindowReached = false;
	return EStreamingAudioResult::Ok;
}

EStreamingAudioResult FStreamingAudioMp3Decoder::DecodeHeld(const bool bFinishing, TArray<uint8>& OutPcm)
{
	EStreamingAudioResult Result = EStreamingAudioResult::Ok;
	int32 Offset = 0;
	while (Offset < Buffer.Num())
	{
		const int32 Remaining = Buffer.Num() - Offset;
		// The decoder confirms a frame by the header that follows it. Without that header it would
		// discard its state and rescan, so a frame waits until the next header has arrived.
		if (!bFinishing && Remaining < DecodeMarginBytes)
		{
			break;
		}
		mp3dec_frame_info_t Info;
		FMemory::Memzero(Info);
		const int32 Samples = mp3dec_decode_frame(&Decoder->Decoder, Buffer.GetData() + Offset, Remaining, Decoder->Samples, &Info);

		if (Info.hz == 0)
		{
			// No complete frame was found. At Finish the rest can never complete.
			if (bFinishing)
			{
				TruncatedBytes += Remaining;
				Offset = Buffer.Num();
				break;
			}
			// Keep the last margin: a frame may start there whose successor has not arrived.
			const int32 Drop = FMath::Min(Info.frame_bytes, Remaining - DecodeMarginBytes);
			if (Drop <= 0)
			{
				break;
			}
			SkippedBytes += Drop;
			Offset += Drop;
			continue;
		}

		SkippedBytes += Info.frame_offset;
		FStreamingAudioMp3Header Header;
		if (ClassifyHeader(MakeArrayView(Buffer.GetData() + Offset + Info.frame_offset, Remaining - Info.frame_offset), Header) != EStreamingAudioResult::Ok)
		{
			Result = EStreamingAudioResult::UnsupportedFormat;
			break;
		}
		if (Samples > 0)
		{
			FStreamingAudioFormat FrameFormat;
			FrameFormat.SampleRate = Info.hz;
			FrameFormat.NumChannels = Info.channels;
			if (!Format.IsValid())
			{
				Format = FrameFormat;
				State = EStreamingAudioMp3State::Decoding;
			}
			else if (FrameFormat != Format)
			{
				Result = EStreamingAudioResult::FormatChanged;
				break;
			}
			const int32 Bytes = Samples * Info.channels * FStreamingAudioFormat::BytesPerSample;
			OutPcm.Append(reinterpret_cast<const uint8*>(Decoder->Samples), Bytes);
		}
		else
		{
			// A frame that produced no audio, such as one whose bit reservoir was lost before a
			// resynchronisation.
			SkippedBytes += Info.frame_bytes - Info.frame_offset;
		}
		Offset += Info.frame_bytes;
	}

	if (Result != EStreamingAudioResult::Ok)
	{
		State = EStreamingAudioMp3State::Failed;
		Buffer.Reset();
		return Result;
	}
	Buffer.RemoveAt(0, Offset, EAllowShrinking::No);
	return Result;
}
