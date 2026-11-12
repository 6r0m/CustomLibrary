#include "Async/Async.h"
#include "BlueprintCompileCheck.h"
#include "Cases/AudioCases.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "Sound/SoundWaveProcedural.h"
#include "StreamingAudioBlueprint.h"
#include "StreamingAudioChunker.h"
#include "StreamingAudioFeeder.h"
#include "StreamingAudioMp3Decoder.h"
#include "StreamingAudioWav.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/StructOnScope.h"
#include <atomic>

IMPLEMENT_MODULE(FDefaultModuleImpl, StreamingAudioTests)

namespace
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	// The procedural wave renders at most this many samples per call and needs at least 512.
	constexpr int32 SamplesPerPull = 1024;

	/** Pulls PCM from a wave that is not playing, as the audio renderer would, until it is drained or MaxBytes is reached. */
	TArray<uint8> Pull(USoundWaveProcedural* Wave, int32 MaxBytes = MAX_int32)
	{
		TArray<uint8> Out;
		uint8 Buffer[SamplesPerPull * 4];
		while (Wave->GetAvailableAudioByteCount() > 0 && Out.Num() < MaxBytes)
		{
			Out.Append(Buffer, Wave->GeneratePCMData(Buffer, SamplesPerPull));
		}
		return Out;
	}

	struct FDecodeRun
	{
		TArray<uint8> Pcm;
		EStreamingAudioResult FirstFailure = EStreamingAudioResult::Ok;
		int64 BytesFedBeforeFirstOutput = -1;
	};

	/** Feeds Bytes to Decoder in fragments of FragmentBytes (0: all at once), then calls Finish. */
	FDecodeRun Decode(FStreamingAudioMp3Decoder& Decoder, const TArray<uint8>& Bytes, int32 FragmentBytes)
	{
		FDecodeRun Run;
		const int32 Step = FragmentBytes > 0 ? FragmentBytes : FMath::Max(Bytes.Num(), 1);
		for (int32 Offset = 0; Offset < Bytes.Num(); Offset += Step)
		{
			TArray<uint8> Pcm;
			const EStreamingAudioResult Result = Decoder.Append(MakeArrayView(Bytes.GetData() + Offset, FMath::Min(Step, Bytes.Num() - Offset)), Pcm);
			if (!Pcm.IsEmpty() && Run.BytesFedBeforeFirstOutput < 0)
			{
				Run.BytesFedBeforeFirstOutput = Offset + FMath::Min(Step, Bytes.Num() - Offset);
			}
			Run.Pcm.Append(Pcm);
			if (Result != EStreamingAudioResult::Ok)
			{
				Run.FirstFailure = Result;
				return Run;
			}
		}
		TArray<uint8> Tail;
		const EStreamingAudioResult Result = Decoder.Finish(Tail);
		Run.Pcm.Append(Tail);
		Run.FirstFailure = Result;
		return Run;
	}

	TUniquePtr<FStreamingAudioFeeder> MakeFeeder(const FStreamingAudioFormat& Format, float MaxQueuedSeconds = 10.0f)
	{
		FStreamingAudioFeederSettings Settings;
		Settings.MaxQueuedSeconds = MaxQueuedSeconds;
		EStreamingAudioResult Result;
		return FStreamingAudioFeeder::Create(Format, Settings, Result);
	}

	/** Offsets and sizes of the frames of a stream with no tags, walked with the plugin's header classifier. */
	TArray<TPair<int32, int32>> WalkFrames(const TArray<uint8>& Bytes)
	{
		TArray<TPair<int32, int32>> Frames;
		for (int32 Offset = 0; Offset + 4 <= Bytes.Num();)
		{
			FStreamingAudioMp3Header Header;
			if (FStreamingAudioMp3Decoder::ClassifyHeader(MakeArrayView(Bytes.GetData() + Offset, 4), Header) != EStreamingAudioResult::Ok)
			{
				break;
			}
			Frames.Emplace(Offset, Header.FrameBytes);
			Offset += Header.FrameBytes;
		}
		return Frames;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioOddFragments, "StreamingAudio.Feeder.S1OddFragments", TestFlags)
bool FAudioOddFragments::RunTest(const FString& Parameters)
{
	const FStreamingAudioFormat Format = AudioCases::MakeFormat(16000, 1);
	const TArray<uint8> Input = AudioCases::Ramp(Format, 1000);
	TUniquePtr<FStreamingAudioFeeder> Feeder = MakeFeeder(Format);
	for (int32 Offset = 0, Piece = 0; Offset < Input.Num(); ++Piece)
	{
		const int32 Size = FMath::Min(Piece % 2 == 0 ? 3 : 5, Input.Num() - Offset);
		TestEqual(TEXT("append"), Feeder->Append(MakeArrayView(Input.GetData() + Offset, Size)), EStreamingAudioResult::Ok);
		Offset += Size;
	}
	TestEqual(TEXT("every whole frame is queued"), Feeder->GetSoundWave()->GetAvailableAudioByteCount(), Input.Num());
	TestTrue(TEXT("queued bytes equal the input"), Pull(Feeder->GetSoundWave()) == Input);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioStereoAlignment, "StreamingAudio.Feeder.S2StereoAlignment", TestFlags)
bool FAudioStereoAlignment::RunTest(const FString& Parameters)
{
	const FStreamingAudioFormat Format = AudioCases::MakeFormat(44100, 2);
	const TArray<uint8> Input = AudioCases::Ramp(Format, 1000);
	TUniquePtr<FStreamingAudioFeeder> Feeder = MakeFeeder(Format);
	Feeder->Append(MakeArrayView(Input.GetData(), 6));
	TestEqual(TEXT("one whole frame queued, half a frame carried"), Feeder->GetSoundWave()->GetAvailableAudioByteCount(), 4);
	Feeder->Append(MakeArrayView(Input.GetData() + 6, Input.Num() - 6));
	TestEqual(TEXT("the carried half completes a frame"), Feeder->GetSoundWave()->GetAvailableAudioByteCount(), Input.Num());
	const uint8 Extra[2] = {1, 2};
	Feeder->Append(MakeArrayView(Extra, 2));
	int32 Dropped = 0;
	TestEqual(TEXT("flush"), Feeder->Flush(Dropped), EStreamingAudioResult::Ok);
	TestEqual(TEXT("flush reports the partial frame"), Dropped, 2);
	TestTrue(TEXT("channels stay in order"), Pull(Feeder->GetSoundWave()) == Input);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioOracle, "StreamingAudio.Mp3.S3Oracle", TestFlags)
bool FAudioOracle::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Manifest = AudioCases::Manifest();
	if (!TestTrue(TEXT("the fixture manifest is present"), Manifest.IsValid()))
	{
		return false;
	}
	const TSharedPtr<FJsonObject> Audio = Manifest->GetObjectField(TEXT("audio"));
	for (const FString& Name : AudioCases::DecodableFixtures())
	{
		const TSharedPtr<FJsonObject> Entry = Audio->GetObjectField(Name);
		const TArray<uint8> Bytes = AudioCases::LoadFixture(Name);
		TestEqual(FString::Printf(TEXT("%s: fixture size"), *Name), Bytes.Num(), static_cast<int32>(Entry->GetIntegerField(TEXT("bytes"))));
		for (const int32 Fragment : {0, AudioCases::FragmentBytes})
		{
			FStreamingAudioMp3Decoder Decoder;
			const FDecodeRun Run = Decode(Decoder, Bytes, Fragment);
			const FString Label = FString::Printf(TEXT("%s in %s"), *Name, Fragment ? TEXT("1 KB fragments") : TEXT("one piece"));
			TestEqual(Label + TEXT(": result"), Run.FirstFailure, EStreamingAudioResult::Ok);
			TestEqual(Label + TEXT(": sample count"), static_cast<int64>(Run.Pcm.Num()), AudioCases::ExpectedPcmBytes(*Entry));
			TestEqual(Label + TEXT(": sample rate"), Decoder.GetFormat().SampleRate, static_cast<int32>(Entry->GetIntegerField(TEXT("sample_rate"))));
			TestEqual(Label + TEXT(": channels"), Decoder.GetFormat().NumChannels, static_cast<int32>(Entry->GetIntegerField(TEXT("channels"))));
			TestEqual(Label + TEXT(": nothing skipped"), Decoder.GetSkippedBytes(), static_cast<int64>(0));
			TestEqual(Label + TEXT(": nothing truncated"), Decoder.GetTruncatedBytes(), static_cast<int64>(0));
			TestEqual(Label + TEXT(": finished"), Decoder.GetState(), EStreamingAudioMp3State::Finished);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioCorruption, "StreamingAudio.Mp3.S4Corruption", TestFlags)
bool FAudioCorruption::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Entry = AudioCases::Manifest()->GetObjectField(TEXT("audio"))->GetObjectField(AudioCases::CorruptedFixture());
	FStreamingAudioMp3Decoder Decoder;
	const FDecodeRun Run = Decode(Decoder, AudioCases::LoadFixture(AudioCases::CorruptedFixture()), AudioCases::FragmentBytes);
	const int32 Corrupted = static_cast<int32>(Entry->GetIntegerField(TEXT("corrupted_frame")));
	const int64 FrameBytes = AudioCases::ExpectedPcmBytes(*Entry) / Entry->GetIntegerField(TEXT("frames"));
	TestEqual(TEXT("the stream decodes to the end"), Run.FirstFailure, EStreamingAudioResult::Ok);
	TestTrue(TEXT("the bad frame is skipped"), Decoder.GetSkippedBytes() > 0);
	TestTrue(TEXT("output resumes after the bad frame"), static_cast<int64>(Run.Pcm.Num()) > (Corrupted + 1) * FrameBytes);
	TestTrue(TEXT("only frames around the bad one are lost"),
		static_cast<int64>(Run.Pcm.Num()) >= AudioCases::ExpectedPcmBytes(*Entry, AudioCases::MaxFramesLostToCorruption));
	TestTrue(TEXT("the bad frame produced no audio"), static_cast<int64>(Run.Pcm.Num()) < AudioCases::ExpectedPcmBytes(*Entry));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioSyncWindow, "StreamingAudio.Mp3.S5SyncWindow", TestFlags)
bool FAudioSyncWindow::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Entry = AudioCases::Manifest()->GetObjectField(TEXT("audio"))->GetObjectField(AudioCases::StereoFixture());
	const TArray<uint8> Bytes = AudioCases::LoadFixture(AudioCases::StereoFixture());
	const double BytesPerSecond = Entry->GetArrayField(TEXT("bitrates_kbps"))[0]->AsNumber() * 1000.0 / 8.0;
	for (const int32 Window : {FStreamingAudioMp3Settings().SyncWindowBytes, AudioCases::SmallSyncWindowBytes})
	{
		FStreamingAudioMp3Settings Settings;
		Settings.SyncWindowBytes = Window;
		FStreamingAudioMp3Decoder Decoder(Settings);
		const FDecodeRun Run = Decode(Decoder, Bytes, AudioCases::FragmentBytes);
		TestTrue(FString::Printf(TEXT("window %d: no output before the window fills"), Window), Run.BytesFedBeforeFirstOutput >= Window);
		TestTrue(FString::Printf(TEXT("window %d: output starts within one fragment of the window"), Window),
			Run.BytesFedBeforeFirstOutput < Window + AudioCases::FragmentBytes);
		TestEqual(FString::Printf(TEXT("window %d: sample count"), Window), static_cast<int64>(Run.Pcm.Num()), AudioCases::ExpectedPcmBytes(*Entry));
		AddInfo(FString::Printf(TEXT("sync window %d bytes: first output after %lld bytes (%.0f ms of %d kbps audio)"), Window,
			Run.BytesFedBeforeFirstOutput, 1000.0 * Run.BytesFedBeforeFirstOutput / BytesPerSecond, static_cast<int32>(BytesPerSecond * 8 / 1000)));
	}
	FStreamingAudioMp3Settings TooSmall;
	TooSmall.SyncWindowBytes = FStreamingAudioMp3Decoder::DecodeMarginBytes - 1;
	FStreamingAudioMp3Decoder Refused(TooSmall);
	TArray<uint8> Pcm;
	TestEqual(TEXT("a window smaller than one frame and a header is refused"), Refused.Append(Bytes, Pcm), EStreamingAudioResult::InvalidSettings);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioHeaderParser, "StreamingAudio.Mp3.S6aHeaderClassifier", TestFlags)
bool FAudioHeaderParser::RunTest(const FString& Parameters)
{
	FStreamingAudioMp3Header Header;
	TestEqual(TEXT("Layer 1 is unsupported"), FStreamingAudioMp3Decoder::ClassifyHeader(AudioCases::LoadFixture(AudioCases::Layer1HeaderFixture()), Header), EStreamingAudioResult::UnsupportedFormat);
	TestEqual(TEXT("Layer 1 is recognised"), Header.Layer, 1);
	TestEqual(TEXT("free format is unsupported"), FStreamingAudioMp3Decoder::ClassifyHeader(AudioCases::LoadFixture(AudioCases::FreeFormatHeaderFixture()), Header), EStreamingAudioResult::UnsupportedFormat);
	TestEqual(TEXT("free format is Layer 3"), Header.Layer, 3);
	TestEqual(TEXT("free format has no bitrate"), Header.BitrateKbps, 0);
	TestEqual(TEXT("a Layer 3 header is supported"), FStreamingAudioMp3Decoder::ClassifyHeader(AudioCases::LoadFixture(AudioCases::StereoFixture()), Header), EStreamingAudioResult::Ok);
	TestEqual(TEXT("its rate"), Header.SampleRate, 44100);
	TestEqual(TEXT("its bitrate"), Header.BitrateKbps, 128);
	TestTrue(TEXT("its frame length"), Header.FrameBytes == 417 || Header.FrameBytes == 418);
	const uint8 NotAHeader[4] = {0x49, 0x44, 0x33, 0x04};
	TestEqual(TEXT("other bytes are not a header"), FStreamingAudioMp3Decoder::ClassifyHeader(MakeArrayView(NotAHeader, 4), Header), EStreamingAudioResult::InvalidInput);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioLayer2, "StreamingAudio.Mp3.S6bLayer2", TestFlags)
bool FAudioLayer2::RunTest(const FString& Parameters)
{
	FStreamingAudioMp3Decoder Decoder;
	const FDecodeRun Run = Decode(Decoder, AudioCases::LoadFixture(AudioCases::Layer2Fixture()), 0);
	TestEqual(TEXT("Layer 2 is refused"), Run.FirstFailure, EStreamingAudioResult::UnsupportedFormat);
	TestEqual(TEXT("no audio"), Run.Pcm.Num(), 0);
	TestEqual(TEXT("the decoder failed"), Decoder.GetState(), EStreamingAudioMp3State::Failed);
	TArray<uint8> Pcm;
	TestEqual(TEXT("it stays failed"), Decoder.Append(AudioCases::LoadFixture(AudioCases::StereoFixture()), Pcm), EStreamingAudioResult::Failed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioFormatChange, "StreamingAudio.Mp3.S6cFormatChange", TestFlags)
bool FAudioFormatChange::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Stereo = AudioCases::Manifest()->GetObjectField(TEXT("audio"))->GetObjectField(AudioCases::StereoFixture());
	TArray<uint8> Bytes = AudioCases::LoadFixture(AudioCases::StereoFixture());
	Bytes.Append(AudioCases::LoadFixture(AudioCases::MonoMpeg2Fixture()));
	FStreamingAudioMp3Decoder Decoder;
	const FDecodeRun Run = Decode(Decoder, Bytes, AudioCases::FragmentBytes);
	TestEqual(TEXT("the change is reported"), Run.FirstFailure, EStreamingAudioResult::FormatChanged);
	TestEqual(TEXT("the decoder failed"), Decoder.GetState(), EStreamingAudioMp3State::Failed);
	TestTrue(TEXT("no audio after the change"), static_cast<int64>(Run.Pcm.Num()) <= AudioCases::ExpectedPcmBytes(*Stereo));
	TestEqual(TEXT("the format stays the first stream's"), Decoder.GetFormat().NumChannels, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioTruncation, "StreamingAudio.Mp3.S7Truncation", TestFlags)
bool FAudioTruncation::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Entry = AudioCases::Manifest()->GetObjectField(TEXT("audio"))->GetObjectField(AudioCases::StereoFixture());
	TArray<uint8> Bytes = AudioCases::LoadFixture(AudioCases::StereoFixture());
	const TArray<TPair<int32, int32>> Frames = WalkFrames(Bytes);
	TestEqual(TEXT("the classifier walks every frame"), Frames.Num(), static_cast<int32>(Entry->GetIntegerField(TEXT("frames"))));
	Bytes.SetNum(Bytes.Num() - AudioCases::TruncatedTailBytes);
	FStreamingAudioMp3Decoder Decoder;
	const FDecodeRun Run = Decode(Decoder, Bytes, AudioCases::FragmentBytes);
	TestEqual(TEXT("finish succeeds"), Run.FirstFailure, EStreamingAudioResult::Ok);
	TestEqual(TEXT("the cut frame is reported"), Decoder.GetTruncatedBytes(), static_cast<int64>(Frames.Last().Value - AudioCases::TruncatedTailBytes));
	TestEqual(TEXT("the cut frame is not decoded"), static_cast<int64>(Run.Pcm.Num()), AudioCases::ExpectedPcmBytes(*Entry, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioChunkDuration, "StreamingAudio.Chunker.S8Duration", TestFlags)
bool FAudioChunkDuration::RunTest(const FString& Parameters)
{
	const FStreamingAudioFormat Format = AudioCases::MakeFormat(16000, 1);
	const TArray<uint8> Pcm = AudioCases::Ramp(Format, 100);
	TArray<TArray<uint8>> Chunks;
	for (const double Seconds : {0.0, -1.0, TNumericLimits<double>::Max() * 2.0})
	{
		TestEqual(FString::Printf(TEXT("duration %f is refused"), Seconds), FStreamingAudioChunker::Split(Format, Pcm, Seconds, Chunks), EStreamingAudioResult::InvalidInput);
		TestEqual(TEXT("no chunks"), Chunks.Num(), 0);
	}
	TestEqual(TEXT("a tiny duration still advances one frame per chunk"), FStreamingAudioChunker::Split(Format, Pcm, 1e-9, Chunks), EStreamingAudioResult::Ok);
	TestEqual(TEXT("one chunk per frame"), Chunks.Num(), 100);
	TestEqual(TEXT("a very large finite duration is accepted"), FStreamingAudioChunker::Split(Format, Pcm, 1.0e300, Chunks), EStreamingAudioResult::Ok);
	TestEqual(TEXT("an oversized duration produces one chunk"), Chunks.Num(), 1);
	TestTrue(TEXT("the oversized chunk preserves every byte"), Chunks.Num() == 1 && Chunks[0] == Pcm);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioChunkAlignment, "StreamingAudio.Chunker.S9Alignment", TestFlags)
bool FAudioChunkAlignment::RunTest(const FString& Parameters)
{
	const FStreamingAudioFormat Format = AudioCases::MakeFormat(44100, 2);
	const TArray<uint8> Pcm = AudioCases::Ramp(Format, 44100);
	TArray<TArray<uint8>> Chunks;
	TestEqual(TEXT("split"), FStreamingAudioChunker::Split(Format, Pcm, 0.3333, Chunks), EStreamingAudioResult::Ok);
	TArray<uint8> Joined;
	for (const TArray<uint8>& Chunk : Chunks)
	{
		TestEqual(TEXT("every chunk is whole frames"), Chunk.Num() % Format.GetBytesPerFrame(), 0);
		Joined.Append(Chunk);
	}
	TestEqual(TEXT("first chunk length"), Chunks[0].Num(), 14698 * Format.GetBytesPerFrame());
	TestTrue(TEXT("chunks reassemble the input"), Joined == Pcm);
	TestEqual(TEXT("partial frames are refused"), FStreamingAudioChunker::Split(Format, MakeArrayView(Pcm.GetData(), 6), 0.1, Chunks), EStreamingAudioResult::InvalidInput);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioCaps, "StreamingAudio.Caps.S10", TestFlags)
bool FAudioCaps::RunTest(const FString& Parameters)
{
	const FStreamingAudioFormat Format = AudioCases::MakeFormat(1000, 1);
	TUniquePtr<FStreamingAudioFeeder> Feeder = MakeFeeder(Format, 1.0f);
	TestEqual(TEXT("cap in bytes"), Feeder->GetMaxQueuedBytes(), 2000);
	const TArray<uint8> Pcm = AudioCases::Ramp(Format, 1000);
	TestEqual(TEXT("below the cap"), Feeder->Append(MakeArrayView(Pcm.GetData(), 1998)), EStreamingAudioResult::Ok);
	TestEqual(TEXT("past the cap"), Feeder->Append(MakeArrayView(Pcm.GetData(), 4)), EStreamingAudioResult::CapExceeded);
	TestEqual(TEXT("a refused append queues nothing"), Feeder->GetSoundWave()->GetAvailableAudioByteCount(), 1998);
	TestEqual(TEXT("exactly the cap"), Feeder->Append(MakeArrayView(Pcm.GetData(), 2)), EStreamingAudioResult::Ok);

	FStreamingAudioMp3Settings Settings;
	Settings.SyncWindowBytes = 2000;
	Settings.MaxBufferedBytes = 2000;
	FStreamingAudioMp3Decoder Decoder(Settings);
	const TArray<uint8> Bytes = AudioCases::LoadFixture(AudioCases::StereoFixture());
	TArray<uint8> Out;
	TestEqual(TEXT("decoder below the cap"), Decoder.Append(MakeArrayView(Bytes.GetData(), 1999), Out), EStreamingAudioResult::Ok);
	TestEqual(TEXT("decoder past the cap"), Decoder.Append(MakeArrayView(Bytes.GetData() + 1999, 2), Out), EStreamingAudioResult::CapExceeded);
	TestEqual(TEXT("a refused append holds nothing more"), Decoder.GetBufferedBytes(), 1999);
	TestEqual(TEXT("still buffering"), Decoder.GetState(), EStreamingAudioMp3State::Buffering);

	FStreamingAudioFeederSettings Bad;
	Bad.MaxQueuedSeconds = 0.0f;
	EStreamingAudioResult Result;
	TestFalse(TEXT("a zero cap is refused"), FStreamingAudioFeeder::Create(Format, Bad, Result).IsValid());
	TestEqual(TEXT("with InvalidSettings"), Result, EStreamingAudioResult::InvalidSettings);
	TestFalse(TEXT("an invalid format is refused"), FStreamingAudioFeeder::Create(AudioCases::MakeFormat(0, 1), FStreamingAudioFeederSettings(), Result).IsValid());
	TestEqual(TEXT("with InvalidInput"), Result, EStreamingAudioResult::InvalidInput);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioWav, "StreamingAudio.Wav.S11", TestFlags)
bool FAudioWav::RunTest(const FString& Parameters)
{
	const FStreamingAudioFormat Format = AudioCases::MakeFormat(22050, 2);
	const TArray<uint8> Pcm = AudioCases::Ramp(Format, 777);
	TArray<uint8> File;
	TestEqual(TEXT("write"), FStreamingAudioWav::Write(Format, Pcm, File), EStreamingAudioResult::Ok);
	FStreamingAudioFormat ReadFormat;
	TArray<uint8> ReadPcm;
	TestEqual(TEXT("read back"), FStreamingAudioWav::Read(File, ReadFormat, ReadPcm), EStreamingAudioResult::Ok);
	TestTrue(TEXT("the format round-trips"), ReadFormat == Format);
	TestTrue(TEXT("the samples round-trip"), ReadPcm == Pcm);

	TestEqual(TEXT("a 16-bit fixture"), FStreamingAudioWav::Read(AudioCases::LoadFixture(AudioCases::Wav16Fixture()), ReadFormat, ReadPcm), EStreamingAudioResult::Ok);
	TestTrue(TEXT("its format"), ReadFormat == AudioCases::MakeFormat(16000, 1));
	TestEqual(TEXT("its length"), ReadPcm.Num(), 8000 * 2);
	TestEqual(TEXT("an extensible 16-bit fixture"), FStreamingAudioWav::Read(AudioCases::LoadFixture(AudioCases::WavExtensibleFixture()), ReadFormat, ReadPcm), EStreamingAudioResult::Ok);
	TestTrue(TEXT("its extensible format"), ReadFormat == AudioCases::MakeFormat(44100, 2));
	TestTrue(TEXT("its extensible PCM"), ReadPcm == AudioCases::Ramp(AudioCases::MakeFormat(44100, 2), 100, 3));
	TestEqual(TEXT("a 24-bit file is refused"), FStreamingAudioWav::Read(AudioCases::LoadFixture(AudioCases::Wav24Fixture()), ReadFormat, ReadPcm), EStreamingAudioResult::UnsupportedFormat);
	TestEqual(TEXT("a refused read returns nothing"), ReadPcm.Num(), 0);
	TestEqual(TEXT("an MP3 is not a WAV file"), FStreamingAudioWav::Read(AudioCases::LoadFixture(AudioCases::StereoFixture()), ReadFormat, ReadPcm), EStreamingAudioResult::InvalidInput);
	TestEqual(TEXT("partial frames are not written"), FStreamingAudioWav::Write(Format, MakeArrayView(Pcm.GetData(), 6), File), EStreamingAudioResult::InvalidInput);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioQueuedSound, "StreamingAudio.Feeder.QueuedSound", TestFlags)
bool FAudioQueuedSound::RunTest(const FString& Parameters)
{
	const FStreamingAudioFormat Format = AudioCases::MakeFormat(16000, 1);
	const TArray<uint8> Pcm = AudioCases::Ramp(Format, 8000);
	USoundWaveProcedural* Wave = nullptr;
	TestEqual(TEXT("create"), FStreamingAudioFeeder::CreateQueuedSound(Format, Pcm, Wave), EStreamingAudioResult::Ok);
	if (!TestNotNull(TEXT("a wave"), Wave))
	{
		return false;
	}
	TestEqual(TEXT("its duration"), Wave->Duration, 0.5f);
	TestEqual(TEXT("its channels"), Wave->NumChannels, 1);
	TestTrue(TEXT("it holds the PCM"), Pull(Wave) == Pcm);
	TestEqual(TEXT("partial frames are refused"), FStreamingAudioFeeder::CreateQueuedSound(AudioCases::MakeFormat(16000, 2), MakeArrayView(Pcm.GetData(), 6), Wave), EStreamingAudioResult::InvalidInput);
	TestNull(TEXT("and create nothing"), Wave);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioLifecycle, "StreamingAudio.Lifecycle.S12", TestFlags)
bool FAudioLifecycle::RunTest(const FString& Parameters)
{
	const FStreamingAudioFormat Format = AudioCases::MakeFormat(16000, 1);
	const TArray<uint8> Pcm = AudioCases::Ramp(Format, 100);
	TUniquePtr<FStreamingAudioFeeder> Feeder = MakeFeeder(Format);
	int32 Dropped = 0;
	Feeder->Append(Pcm);
	TestEqual(TEXT("flush"), Feeder->Flush(Dropped), EStreamingAudioResult::Ok);
	TestEqual(TEXT("append after flush"), Feeder->Append(Pcm), EStreamingAudioResult::Finished);
	TestEqual(TEXT("flush after flush"), Feeder->Flush(Dropped), EStreamingAudioResult::Finished);
	USoundWaveProcedural* Old = Feeder->GetSoundWave();
	TestEqual(TEXT("reset"), Feeder->Reset(), EStreamingAudioResult::Ok);
	TestNotEqual(TEXT("reset starts a new wave"), Feeder->GetSoundWave(), Old);
	TestEqual(TEXT("append after reset"), Feeder->Append(Pcm), EStreamingAudioResult::Ok);
	TestTrue(TEXT("the new wave holds only the new stream"), Pull(Feeder->GetSoundWave()) == Pcm);

	const TArray<uint8> Bytes = AudioCases::LoadFixture(AudioCases::StereoFixture());
	FStreamingAudioMp3Decoder Decoder;
	const FDecodeRun First = Decode(Decoder, Bytes, AudioCases::FragmentBytes);
	TArray<uint8> Out;
	TestEqual(TEXT("append after finish"), Decoder.Append(Bytes, Out), EStreamingAudioResult::Finished);
	TestEqual(TEXT("finish after finish"), Decoder.Finish(Out), EStreamingAudioResult::Finished);
	TestEqual(TEXT("decoder reset"), Decoder.Reset(), EStreamingAudioResult::Ok);
	TestEqual(TEXT("reset returns to buffering"), Decoder.GetState(), EStreamingAudioMp3State::Buffering);
	TestFalse(TEXT("reset clears the format"), Decoder.GetFormat().IsValid());
	const FDecodeRun Second = Decode(Decoder, Bytes, AudioCases::FragmentBytes);
	TestTrue(TEXT("reuse decodes the same audio, with nothing stale"), First.Pcm == Second.Pcm && !First.Pcm.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioResetInterleaving, "StreamingAudio.Lifecycle.S12rResetInterleaving", TestFlags)
bool FAudioResetInterleaving::RunTest(const FString& Parameters)
{
	const FStreamingAudioFormat Format = AudioCases::MakeFormat(16000, 1);
	const TArray<uint8> A = AudioCases::Ramp(Format, 1000, 1);
	// Longer than one render call, so the first pull is partial.
	const TArray<uint8> B = AudioCases::Ramp(Format, 3 * SamplesPerPull, 2);
	TUniquePtr<FStreamingAudioFeeder> Feeder = MakeFeeder(Format, 1.0f);
	TStrongObjectPtr<USoundWaveProcedural> Old(Feeder->GetSoundWave());
	Feeder->Append(A);
	Feeder->Reset();
	USoundWaveProcedural* New = Feeder->GetSoundWave();
	TestNotEqual(TEXT("reset returns a different wave"), New, Old.Get());
	Feeder->Append(B);
	TestEqual(TEXT("the new wave counts exactly B"), New->GetAvailableAudioByteCount(), B.Num());
	const TArray<uint8> Part = Pull(New, 1);
	TestEqual(TEXT("after a partial pull the count is B minus what played"), New->GetAvailableAudioByteCount(), B.Num() - Part.Num());
	TArray<uint8> Played = Part;
	Played.Append(Pull(New));
	TestTrue(TEXT("the new wave plays exactly B"), Played == B);
	TestEqual(TEXT("the new wave's count never goes negative"), New->GetAvailableAudioByteCount(), 0);
	TestEqual(TEXT("the old wave received nothing after the reset"), Old->GetAvailableAudioByteCount(), A.Num());
	TestTrue(TEXT("the old wave plays out what it had"), Pull(Old.Get()) == A);

	// The cap holds after a reset: exactly the cap is admitted, one frame more is refused.
	Feeder->Reset();
	const TArray<uint8> Full = AudioCases::Ramp(Format, Feeder->GetMaxQueuedBytes() / Format.GetBytesPerFrame(), 3);
	TestEqual(TEXT("exactly the cap after a reset"), Feeder->Append(Full), EStreamingAudioResult::Ok);
	TestEqual(TEXT("one frame past the cap"), Feeder->Append(MakeArrayView(Full.GetData(), Format.GetBytesPerFrame())), EStreamingAudioResult::CapExceeded);
	TestEqual(TEXT("the refused frame was not queued"), Feeder->GetSoundWave()->GetAvailableAudioByteCount(), Feeder->GetMaxQueuedBytes());

	// Control: reusing one wave across an engine reset breaks its byte count, which is why the
	// feeder never does it.
	TStrongObjectPtr<USoundWaveProcedural> Naive(NewObject<USoundWaveProcedural>());
	Naive->QueueAudio(A.GetData(), A.Num());
	Naive->ResetAudio();
	Naive->QueueAudio(B.GetData(), B.Num());
	uint8 Buffer[SamplesPerPull * 4];
	Naive->GeneratePCMData(Buffer, SamplesPerPull);
	TestTrue(TEXT("control: the naive sequence drives the count negative"), Naive->GetAvailableAudioByteCount() < 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioLiveHandoff, "StreamingAudio.Lifecycle.S12pLiveHandoff", TestFlags)
bool FAudioLiveHandoff::RunTest(const FString& Parameters)
{
	const FStreamingAudioFormat Format = AudioCases::MakeFormat(16000, 1);
	const TArray<uint8> A = AudioCases::Ramp(Format, 700, 1);
	const TArray<uint8> B = AudioCases::Ramp(Format, 300, 2);
	TUniquePtr<FStreamingAudioFeeder> Feeder = MakeFeeder(Format);

	// The worker stands in for the audio renderer: it is the only consumer of whichever wave it is given.
	std::atomic<USoundWaveProcedural*> Target{nullptr};
	std::atomic<int32> Acknowledged{0};
	std::atomic<int32> Requested{0};
	std::atomic<bool> bStop{false};
	TFuture<void> Worker = Async(EAsyncExecution::Thread, [&]()
	{
		uint8 Buffer[SamplesPerPull * 4];
		while (!bStop)
		{
			Acknowledged = Requested.load();
			if (USoundWaveProcedural* Wave = Target.load())
			{
				Wave->GeneratePCMData(Buffer, SamplesPerPull);
			}
			else
			{
				FPlatformProcess::Yield();
			}
		}
	});
	auto Retarget = [&](USoundWaveProcedural* Wave)
	{
		Target = Wave;
		const int32 Ticket = ++Requested;
		while (Acknowledged.load() < Ticket)
		{
			FPlatformProcess::Yield();
		}
	};

	TArray<TStrongObjectPtr<USoundWaveProcedural>> Waves;
	int32 NewCountWrong = 0, OldCountRose = 0, CurrentCounterMoved = 0;
	for (int32 Cycle = 0; Cycle < AudioCases::HandoffCycles; ++Cycle)
	{
		USoundWaveProcedural* Live = Feeder->GetSoundWave();
		Waves.Emplace(Live);
		Target = Live;
		Feeder->Append(A);
		Feeder->Reset();
		const int32 AtReset = Live->GetAvailableAudioByteCount();
		USoundWaveProcedural* Next = Feeder->GetSoundWave();
		Feeder->Append(B);
		NewCountWrong += Next->GetAvailableAudioByteCount() != B.Num();
		CurrentCounterMoved += Feeder->GetUnderflowCount() != 0;
		OldCountRose += Live->GetAvailableAudioByteCount() > AtReset;
	}

	// Control: a feeder that kept queuing to the wave it handed off would be caught by the same check.
	USoundWaveProcedural* Handed = Feeder->GetSoundWave();
	Waves.Emplace(Handed);
	Feeder->Reset();
	Retarget(nullptr);
	const int32 AtReset = Handed->GetAvailableAudioByteCount();
	Handed->QueueAudio(A.GetData(), A.Num());
	const bool bControlCaught = Handed->GetAvailableAudioByteCount() > AtReset;

	bStop = true;
	Worker.Wait();
	int32 Negative = 0;
	for (const TStrongObjectPtr<USoundWaveProcedural>& Wave : Waves)
	{
		Negative += Wave->GetAvailableAudioByteCount() < 0;
	}
	TestEqual(TEXT("each new wave holds exactly what was queued to it"), NewCountWrong, 0);
	TestEqual(TEXT("a handed-off wave never gains audio"), OldCountRose, 0);
	TestEqual(TEXT("underflows on handed-off waves never reach the current counter"), CurrentCounterMoved, 0);
	TestEqual(TEXT("no count goes negative"), Negative, 0);
	TestTrue(TEXT("control: queuing to a handed-off wave is detected"), bControlCaught);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioUnderflowAndThread, "StreamingAudio.Lifecycle.S13UnderflowAndThread", TestFlags)
bool FAudioUnderflowAndThread::RunTest(const FString& Parameters)
{
	const FStreamingAudioFormat Format = AudioCases::MakeFormat(16000, 1);
	TUniquePtr<FStreamingAudioFeeder> Feeder = MakeFeeder(Format);
	uint8 Buffer[SamplesPerPull * 4];
	Feeder->GetSoundWave()->GeneratePCMData(Buffer, SamplesPerPull);
	Feeder->GetSoundWave()->GeneratePCMData(Buffer, SamplesPerPull);
	TestEqual(TEXT("each starved render counts an underflow"), Feeder->GetUnderflowCount(), 2);
	Feeder->Reset();
	TestEqual(TEXT("a new wave starts a new count"), Feeder->GetUnderflowCount(), 0);

	const TArray<uint8> Pcm = AudioCases::Ramp(Format, 10);
	FStreamingAudioMp3Decoder Decoder;
	const TArray<EStreamingAudioResult> Results = Async(EAsyncExecution::Thread, [&]()
	{
		int32 Dropped = 0;
		TArray<uint8> Out;
		EStreamingAudioResult Created = EStreamingAudioResult::Ok;
		const bool bCreated = FStreamingAudioFeeder::Create(Format, FStreamingAudioFeederSettings(), Created).IsValid();
		USoundWaveProcedural* Wave = nullptr;
		return TArray<EStreamingAudioResult>{
			Feeder->Append(Pcm), Feeder->Flush(Dropped), Feeder->Reset(), Decoder.Append(Pcm, Out), Decoder.Finish(Out), Decoder.Reset(),
			bCreated ? EStreamingAudioResult::Ok : Created, FStreamingAudioFeeder::CreateQueuedSound(Format, Pcm, Wave)};
	}).Get();
	for (const EStreamingAudioResult Result : Results)
	{
		TestEqual(TEXT("a call from another thread is refused"), Result, EStreamingAudioResult::WrongThread);
	}
	TestEqual(TEXT("the refused append queued nothing"), Feeder->GetSoundWave()->GetAvailableAudioByteCount(), 0);
	TestFalse(TEXT("the refused flush did not finish the stream"), Feeder->IsFinished());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioConcurrentUnderflowGetter, "StreamingAudio.Lifecycle.S14ConcurrentUnderflowGetter", TestFlags)
bool FAudioConcurrentUnderflowGetter::RunTest(const FString& Parameters)
{
	const FStreamingAudioFormat Format = AudioCases::MakeFormat(16000, 1);
	TUniquePtr<FStreamingAudioFeeder> Feeder = MakeFeeder(Format);
	std::atomic<bool> bStart{false};
	std::atomic<bool> bStop{false};
	TFuture<void> Worker = Async(EAsyncExecution::Thread, [&]()
	{
		while (!bStart)
		{
			FPlatformProcess::Yield();
		}
		while (!bStop)
		{
			(void)Feeder->GetUnderflowCount();
			FPlatformProcess::Yield();
		}
	});
	bStart = true;
	for (int32 Cycle = 0; Cycle < 1000; ++Cycle)
	{
		Feeder->Reset();
		(void)Feeder->GetUnderflowCount();
	}
	bStop = true;
	Worker.Wait();
	TestEqual(TEXT("the getter survives concurrent resets"), Feeder->GetUnderflowCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioReflectedCall, "StreamingAudio.Blueprint.ReflectedCall", TestFlags)
bool FAudioReflectedCall::RunTest(const FString& Parameters)
{
	UFunction* Function = UStreamingAudioBlueprintLibrary::StaticClass()->FindFunctionByName(TEXT("CreateQueuedSound"));
	if (!TestNotNull(TEXT("CreateQueuedSound is reflected"), Function))
	{
		return false;
	}
	const FStreamingAudioFormat Format = AudioCases::MakeFormat(16000, 1);
	const TArray<uint8> Pcm = AudioCases::Ramp(Format, 160);
	FStructOnScope Params(Function);
	*CastField<FStructProperty>(Function->FindPropertyByName(TEXT("Format")))->ContainerPtrToValuePtr<FStreamingAudioFormat>(Params.GetStructMemory()) = Format;
	*CastField<FArrayProperty>(Function->FindPropertyByName(TEXT("Pcm")))->ContainerPtrToValuePtr<TArray<uint8>>(Params.GetStructMemory()) = Pcm;
	UStreamingAudioBlueprintLibrary::StaticClass()->GetDefaultObject()->ProcessEvent(Function, Params.GetStructMemory());
	USoundWaveProcedural* Wave = Cast<USoundWaveProcedural>(CastField<FObjectProperty>(Function->FindPropertyByName(TEXT("OutWave")))->GetObjectPropertyValue_InContainer(Params.GetStructMemory()));
	const uint8 Result = *CastField<FEnumProperty>(Function->GetReturnProperty())->ContainerPtrToValuePtr<uint8>(Params.GetStructMemory());
	TestEqual(TEXT("reflected result"), Result, static_cast<uint8>(EStreamingAudioResult::Ok));
	TestTrue(TEXT("reflected call returns the wave"), Wave && Pull(Wave) == Pcm);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioBlueprintCompile, "StreamingAudio.Blueprint.GraphCompiles", TestFlags)
bool FAudioBlueprintCompile::RunTest(const FString& Parameters)
{
	TArray<UFunction*> Functions = HostTestSupport::BlueprintCallableFunctions(UStreamingAudioBlueprintLibrary::StaticClass());
	Functions.Append(HostTestSupport::BlueprintCallableFunctions(UStreamingAudioFeederObject::StaticClass()));
	Functions.Append(HostTestSupport::BlueprintCallableFunctions(UStreamingAudioMp3DecoderObject::StaticClass()));
	TestEqual(TEXT("the Blueprint surface"), Functions.Num(), 2 + 6 + 5);
	FString Messages;
	TestTrue(FString::Printf(TEXT("a graph calling every function compiles:\n%s"), *Messages), HostTestSupport::CompileCallsInBlueprint(Functions, Messages));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioExample, "StreamingAudio.Example.StreamedMp3ToQueuedPlayback", TestFlags)
bool FAudioExample::RunTest(const FString& Parameters)
{
	// Example: MP3 bytes arrive from the network in pieces. Each piece is decoded as it arrives,
	// and the feeder is created once the first frame reveals the format. The caller plays
	// Feeder->GetSoundWave() on an audio component.
	const TArray<uint8> Network = AudioCases::LoadFixture(AudioCases::StereoFixture());
	FStreamingAudioMp3Decoder Decoder;
	TUniquePtr<FStreamingAudioFeeder> Feeder;
	int64 Decoded = 0;
	auto Play = [&](const TArray<uint8>& Pcm)
	{
		if (Pcm.IsEmpty())
		{
			return;
		}
		if (!Feeder)
		{
			EStreamingAudioResult Created;
			Feeder = FStreamingAudioFeeder::Create(Decoder.GetFormat(), FStreamingAudioFeederSettings(), Created);
		}
		Decoded += Pcm.Num();
		Feeder->Append(Pcm);
	};
	for (int32 Offset = 0; Offset < Network.Num(); Offset += AudioCases::FragmentBytes)
	{
		TArray<uint8> Pcm;
		Decoder.Append(MakeArrayView(Network.GetData() + Offset, FMath::Min(AudioCases::FragmentBytes, Network.Num() - Offset)), Pcm);
		Play(Pcm);
	}
	TArray<uint8> Tail;
	Decoder.Finish(Tail);
	Play(Tail);
	int32 Dropped = 0;
	if (!TestTrue(TEXT("a feeder was created"), Feeder.IsValid()))
	{
		return false;
	}
	Feeder->Flush(Dropped);
	TestEqual(TEXT("every decoded frame is queued"), static_cast<int64>(Feeder->GetSoundWave()->GetAvailableAudioByteCount()), Decoded);
	TestEqual(TEXT("decoded audio is whole frames"), Dropped, 0);
	return true;
}
