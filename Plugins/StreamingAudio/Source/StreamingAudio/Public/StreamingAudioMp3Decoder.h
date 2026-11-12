#pragma once

#include "CoreMinimal.h"
#include "StreamingAudioTypes.h"
#include "StreamingAudioMp3Decoder.generated.h"

USTRUCT(BlueprintType)
struct STREAMINGAUDIO_API FStreamingAudioMp3Settings
{
	GENERATED_BODY()

	/**
	 * Bytes buffered before the first decode attempt. A smaller window starts output sooner and
	 * raises the chance of locking onto a false frame header in leading non-audio data. At least
	 * FStreamingAudioMp3Decoder::DecodeMarginBytes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming Audio")
	int32 SyncWindowBytes = 16 * 1024;

	/** Upper bound on undecoded bytes held. At least SyncWindowBytes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming Audio")
	int32 MaxBufferedBytes = 1024 * 1024;
};

UENUM(BlueprintType)
enum class EStreamingAudioMp3State : uint8
{
	/** Waiting for the sync window to fill, or for the first decodable frame. */
	Buffering,
	Decoding,
	/** Finish was called. */
	Finished,
	/** An unsupported frame or a format change ended the stream. */
	Failed,
};

/** Fields of one MPEG audio frame header. */
struct FStreamingAudioMp3Header
{
	/** 10 for MPEG-1, 20 for MPEG-2, 25 for MPEG-2.5. */
	int32 MpegVersionTimesTen = 0;
	int32 Layer = 0;
	/** 0 for a free-format stream. */
	int32 BitrateKbps = 0;
	int32 SampleRate = 0;
	int32 NumChannels = 0;
	int32 SamplesPerFrame = 0;
	/** Frame length including the header and padding; 0 when the header does not determine it. */
	int32 FrameBytes = 0;
};

/**
 * Decodes an MP3 (MPEG-1, MPEG-2 or MPEG-2.5 Layer 3; CBR or VBR; mono or stereo) byte stream
 * that arrives in pieces into interleaved signed 16-bit PCM.
 *
 * States: Buffering, then Decoding once a frame decodes, then Finished after Finish. Failed is
 * terminal until Reset. The decoder belongs to the thread that created it; calls from other
 * threads return WrongThread.
 *
 * Data that does not form a frame is skipped and counted. A frame that has arrived but whose
 * following header has not is held until the next Append or Finish, so the decoder never
 * resynchronises inside a valid stream.
 */
class STREAMINGAUDIO_API FStreamingAudioMp3Decoder
{
public:
	/** Largest Layer 3 frame this decoder accepts: 320 kbps at 32 kHz, padded. */
	static constexpr int32 MaxFrameBytes = 1441;
	/** Bytes kept undecoded until Finish: one whole frame plus the next frame's header. */
	static constexpr int32 DecodeMarginBytes = MaxFrameBytes + 4;

	explicit FStreamingAudioMp3Decoder(const FStreamingAudioMp3Settings& InSettings = FStreamingAudioMp3Settings());
	~FStreamingAudioMp3Decoder();

	/**
	 * Adds bytes and returns the PCM of every frame they completed. CapExceeded leaves everything
	 * unchanged. UnsupportedFormat (a Layer 1 or 2 frame, or a free-format stream) and
	 * FormatChanged (a sample rate or channel count different from the first frame's) move the
	 * decoder to Failed; OutPcm then holds the PCM decoded before the failing frame.
	 */
	EStreamingAudioResult Append(TConstArrayView<uint8> Bytes, TArray<uint8>& OutPcm);

	/** Decodes everything still held and ends the stream. Bytes that do not form a whole frame are counted as truncated. */
	EStreamingAudioResult Finish(TArray<uint8>& OutPcm);

	/** Returns to Buffering and clears the stream, its format and its counters. */
	EStreamingAudioResult Reset();

	EStreamingAudioMp3State GetState() const { return State; }
	/** Format of the PCM; not valid until the first frame decodes. */
	const FStreamingAudioFormat& GetFormat() const { return Format; }
	int64 GetSkippedBytes() const { return SkippedBytes; }
	int64 GetTruncatedBytes() const { return TruncatedBytes; }
	int32 GetBufferedBytes() const { return Buffer.Num(); }

	/**
	 * Reads the first four bytes as a frame header. Returns Ok for a Layer 3 header this decoder
	 * supports, UnsupportedFormat for Layer 1, Layer 2 or a free-format bitrate (OutHeader still
	 * filled), and InvalidInput for bytes that are not a frame header. Any thread.
	 */
	static EStreamingAudioResult ClassifyHeader(TConstArrayView<uint8> HeaderBytes, FStreamingAudioMp3Header& OutHeader);

private:
	struct FDecoderState;

	EStreamingAudioResult CheckCaller() const;
	EStreamingAudioResult DecodeHeld(bool bFinishing, TArray<uint8>& OutPcm);

	FStreamingAudioMp3Settings Settings;
	TUniquePtr<FDecoderState> Decoder;
	TArray<uint8> Buffer;
	FStreamingAudioFormat Format;
	EStreamingAudioMp3State State = EStreamingAudioMp3State::Buffering;
	int64 SkippedBytes = 0;
	int64 TruncatedBytes = 0;
	uint32 OwnerThreadId = 0;
	bool bValidSettings = false;
	bool bSyncWindowReached = false;
};
