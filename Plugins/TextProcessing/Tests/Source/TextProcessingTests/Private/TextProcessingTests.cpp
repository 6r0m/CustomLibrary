#include "Async/Async.h"
#include "BlueprintCompileCheck.h"
#include "Cases/RegexCases.h"
#include "Cases/TextCases.h"
#include "TextProcessingFragmenter.h"
#include "TextProcessingLibrary.h"
#include "TextProcessingFragmenterObject.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "UObject/StructOnScope.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, TextProcessingTests)

namespace
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	struct FStreamResult
	{
		TArray<FString> Fragments;
		FString Tail;
		bool bAllOk = true;
	};

	FStreamResult Feed(FTextProcessingFragmenter& Fragmenter, const TArray<FString>& Deltas)
	{
		FStreamResult Result;
		for (const FString& Delta : Deltas)
		{
			TArray<FString> Emitted;
			Result.bAllOk &= Fragmenter.Append(Delta, Emitted) == ETextProcessingResult::Ok;
			Result.Fragments.Append(Emitted);
		}
		Result.bAllOk &= Fragmenter.Flush(Result.Tail) == ETextProcessingResult::Ok;
		return Result;
	}

	FString Join(const TArray<FString>& Parts)
	{
		return FString::Join(Parts, TEXT("|"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextProcessingStreams, "TextProcessing.Contract.StreamCases", TestFlags)
bool FTextProcessingStreams::RunTest(const FString& Parameters)
{
	for (const TextCases::FStreamCase& Case : TextCases::StreamCases())
	{
		FTextProcessingFragmenter Fragmenter;
		const FStreamResult Result = Feed(Fragmenter, Case.Deltas);
		TestTrue(FString::Printf(TEXT("%s: every call succeeds"), Case.Name), Result.bAllOk);
		TestEqual(FString::Printf(TEXT("%s: fragments"), Case.Name), Join(Result.Fragments), Join(Case.Fragments));
		TestEqual(FString::Printf(TEXT("%s: tail"), Case.Name), Result.Tail, Case.Tail);
		for (const FString& Fragment : Result.Fragments)
		{
			TestFalse(FString::Printf(TEXT("%s: no empty fragment"), Case.Name), Fragment.IsEmpty());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextProcessingReset, "TextProcessing.Contract.X5Reset", TestFlags)
bool FTextProcessingReset::RunTest(const FString& Parameters)
{
	FTextProcessingFragmenter Fragmenter;
	TArray<FString> Emitted;
	TestEqual(TEXT("append"), Fragmenter.Append(TEXT("Hello wor"), Emitted), ETextProcessingResult::Ok);
	TestEqual(TEXT("reset"), Fragmenter.Reset(), ETextProcessingResult::Ok);
	TestEqual(TEXT("nothing pending after reset"), Fragmenter.GetPendingLength(), 0);
	const FStreamResult Result = Feed(Fragmenter, {TEXT("Bye. "), TEXT("Now")});
	TestEqual(TEXT("only text after the reset is emitted"), Join(Result.Fragments), FString(TEXT("Bye.")));
	TestEqual(TEXT("tail after the reset"), Result.Tail, FString(TEXT(" Now")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextProcessingCap, "TextProcessing.Contract.X7Cap", TestFlags)
bool FTextProcessingCap::RunTest(const FString& Parameters)
{
	FTextProcessingSettings Settings;
	Settings.MaxPendingChars = 8;
	FTextProcessingFragmenter Fragmenter(Settings);
	TArray<FString> Emitted;
	TestEqual(TEXT("within the cap"), Fragmenter.Append(TEXT("abcdef"), Emitted), ETextProcessingResult::Ok);

	// The raw batch is larger than the cap, but extracting its fragment leaves a small tail.
	TestEqual(TEXT("a completed batch over the raw cap is admitted"), Fragmenter.Append(TEXT("gh. j"), Emitted), ETextProcessingResult::Ok);
	TestEqual(TEXT("the completed fragment"), Join(Emitted), FString(TEXT("abcdefgh.")));
	TestEqual(TEXT("the retained tail"), Fragmenter.GetPendingLength(), 2);

	// Genuine overflow: no delimiter, so the prospective tail itself exceeds the cap.
	Emitted.Reset();
	Emitted.Add(TEXT("stale"));
	TestEqual(TEXT("a tail past the cap"), Fragmenter.Append(TEXT("1234567"), Emitted), ETextProcessingResult::CapExceeded);
	TestEqual(TEXT("a refused call emits nothing"), Emitted.Num(), 0);
	TestEqual(TEXT("a refused call keeps the pending text"), Fragmenter.GetPendingLength(), 2);

	TestEqual(TEXT("exactly the cap is admitted"), Fragmenter.Append(TEXT("123456"), Emitted), ETextProcessingResult::Ok);
	TestEqual(TEXT("pending text is at the cap"), Fragmenter.GetPendingLength(), 8);
	TestEqual(TEXT("a large completed batch is admitted"), Fragmenter.Append(TEXT("a. b. c."), Emitted), ETextProcessingResult::Ok);
	TestTrue(TEXT("the large batch leaves a tail within the cap"), Fragmenter.GetPendingLength() <= Settings.MaxPendingChars);

	FString LargeCompleted;
	LargeCompleted.Reserve(4096 * 3);
	for (int32 Index = 0; Index < 2048; ++Index)
	{
		LargeCompleted += TEXT("ab. ");
	}
	TestEqual(TEXT("a very large completed batch is admitted"), Fragmenter.Append(LargeCompleted, Emitted), ETextProcessingResult::Ok);
	TestTrue(TEXT("the very large batch leaves a bounded tail"), Fragmenter.GetPendingLength() <= Settings.MaxPendingChars);
	const int32 TailBeforeOversizedDelta = Fragmenter.GetPendingLength();
	FString Unfinished;
	Unfinished.Reserve(100000);
	for (int32 Index = 0; Index < 100000; ++Index)
	{
		Unfinished.AppendChar(TEXT('x'));
	}
	TestEqual(TEXT("an oversized unfinished delta is refused"), Fragmenter.Append(Unfinished, Emitted), ETextProcessingResult::CapExceeded);
	TestEqual(TEXT("the refused oversized delta leaves the tail unchanged"), Fragmenter.GetPendingLength(), TailBeforeOversizedDelta);

	FString Tail;
	TestEqual(TEXT("flush"), Fragmenter.Flush(Tail), ETextProcessingResult::Ok);
	TestFalse(TEXT("the refused delta never reached the stream"), Tail.Contains(TEXT("1234567")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextProcessingRegex, "TextProcessing.Regex.L4", TestFlags)
bool FTextProcessingRegex::RunTest(const FString& Parameters)
{
	for (const RegexCases::FRegexCase& Case : RegexCases::Cases())
	{
		FString Result;
		const bool bCompiled = UTextProcessingLibrary::ReplaceRegexMatches(Case.Input, Case.Pattern, Case.Replacement, Result);
		TestEqual(FString::Printf(TEXT("%s: pattern validity"), Case.Name), bCompiled, Case.bValidPattern);
		TestEqual(FString::Printf(TEXT("%s: result"), Case.Name), Result, FString(Case.Expected));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextProcessingPartition, "TextProcessing.Contract.X9Partition", TestFlags)
bool FTextProcessingPartition::RunTest(const FString& Parameters)
{
	const FString Text = TextCases::PartitionText();
	FTextProcessingFragmenter Whole;
	const FStreamResult Reference = Feed(Whole, {Text});
	TestEqual(TEXT("the reference reproduces the text"), FString::Join(Reference.Fragments, TEXT("")) + Reference.Tail, Text);
	TestEqual(TEXT("the reference fragment count"), Reference.Fragments.Num(), 10);

	FRandomStream Stream(TextCases::PartitionSeed);
	int32 Mismatches = 0;
	for (int32 Run = 0; Run < TextCases::PartitionRuns; ++Run)
	{
		TArray<FString> Deltas;
		for (int32 Start = 0; Start < Text.Len();)
		{
			const int32 Length = FMath::Min(Stream.RandRange(0, TextCases::MaxPieceLength), Text.Len() - Start);
			Deltas.Add(Text.Mid(Start, Length));
			Start += Length;
		}
		FTextProcessingFragmenter Fragmenter;
		const FStreamResult Result = Feed(Fragmenter, Deltas);
		const bool bSame = Result.bAllOk && Result.Fragments == Reference.Fragments && Result.Tail == Reference.Tail;
		if (!bSame && Mismatches++ == 0)
		{
			AddError(FString::Printf(TEXT("run %d: deltas [%s] gave [%s] + '%s'"), Run, *Join(Deltas), *Join(Result.Fragments), *Result.Tail));
		}
	}
	TestEqual(TEXT("every partition gives the reference fragments"), Mismatches, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextProcessingThread, "TextProcessing.Contract.X10WrongThread", TestFlags)
bool FTextProcessingThread::RunTest(const FString& Parameters)
{
	FTextProcessingFragmenter Fragmenter;
	TArray<FString> Emitted;
	Fragmenter.Append(TEXT("Hello"), Emitted);
	// A dedicated thread, so the call can never run inline on the owning thread.
	const TArray<ETextProcessingResult> Results = Async(EAsyncExecution::Thread, [&Fragmenter]()
	{
		TArray<FString> WorkerEmitted;
		FString WorkerTail;
		return TArray<ETextProcessingResult>{
			Fragmenter.Append(TEXT(" world. Next"), WorkerEmitted), Fragmenter.Flush(WorkerTail), Fragmenter.Reset()};
	}).Get();
	for (const ETextProcessingResult Result : Results)
	{
		TestEqual(TEXT("a call from another thread is refused"), Result, ETextProcessingResult::WrongThread);
	}
	FString Tail;
	TestEqual(TEXT("flush on the owning thread"), Fragmenter.Flush(Tail), ETextProcessingResult::Ok);
	TestEqual(TEXT("refused calls changed nothing"), Tail, FString(TEXT("Hello")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextProcessingSettingsCheck, "TextProcessing.Contract.InvalidSettings", TestFlags)
bool FTextProcessingSettingsCheck::RunTest(const FString& Parameters)
{
	FTextProcessingSettings NoDelimiters;
	NoDelimiters.Delimiters.Reset();
	FTextProcessingSettings NoCap;
	NoCap.MaxPendingChars = 0;
	FTextProcessingSettings Surrogate;
	Surrogate.Delimiters = TEXT("\xD83D");
	for (const FTextProcessingSettings& Settings : {NoDelimiters, NoCap, Surrogate})
	{
		FTextProcessingFragmenter Fragmenter(Settings);
		TArray<FString> Emitted;
		FString Tail;
		TestEqual(TEXT("append is refused"), Fragmenter.Append(TEXT("A. B"), Emitted), ETextProcessingResult::InvalidSettings);
		TestEqual(TEXT("flush is refused"), Fragmenter.Flush(Tail), ETextProcessingResult::InvalidSettings);
		TestEqual(TEXT("nothing is held"), Fragmenter.GetPendingLength(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextProcessingReflectedCall, "TextProcessing.Blueprint.ReflectedCall", TestFlags)
bool FTextProcessingReflectedCall::RunTest(const FString& Parameters)
{
	UTextProcessingFragmenterObject* Object = UTextProcessingFragmenterObject::CreateFragmenter(FTextProcessingSettings());
	UFunction* Append = Object->FindFunction(TEXT("Append"));
	if (!TestNotNull(TEXT("Append is reflected"), Append))
	{
		return false;
	}
	// Call Append the way a Blueprint graph does: through its reflected parameter block.
	FStructOnScope Params(Append);
	CastField<FStrProperty>(Append->FindPropertyByName(TEXT("Delta")))->SetPropertyValue_InContainer(Params.GetStructMemory(), TEXT("One. Two"));
	Object->ProcessEvent(Append, Params.GetStructMemory());
	const TArray<FString>& Fragments = *CastField<FArrayProperty>(Append->FindPropertyByName(TEXT("OutFragments")))->ContainerPtrToValuePtr<TArray<FString>>(Params.GetStructMemory());
	const uint8 Result = *CastField<FEnumProperty>(Append->GetReturnProperty())->ContainerPtrToValuePtr<uint8>(Params.GetStructMemory());
	TestEqual(TEXT("reflected result"), Result, static_cast<uint8>(ETextProcessingResult::Ok));
	TestEqual(TEXT("reflected fragments"), Join(Fragments), FString(TEXT("One.")));
	FString Tail;
	TestEqual(TEXT("flush through the wrapper"), Object->Flush(Tail), ETextProcessingResult::Ok);
	TestEqual(TEXT("tail through the wrapper"), Tail, FString(TEXT(" Two")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextProcessingBlueprintCompile, "TextProcessing.Blueprint.GraphCompiles", TestFlags)
bool FTextProcessingBlueprintCompile::RunTest(const FString& Parameters)
{
	TArray<UFunction*> Functions = HostTestSupport::BlueprintCallableFunctions(UTextProcessingFragmenterObject::StaticClass());
	Functions.Append(HostTestSupport::BlueprintCallableFunctions(UTextProcessingLibrary::StaticClass()));
	TestEqual(TEXT("the fragmenter and regex nodes are callable"), Functions.Num(), 5);
	FString Messages;
	TestTrue(FString::Printf(TEXT("a graph calling every function compiles:\n%s"), *Messages), HostTestSupport::CompileCallsInBlueprint(Functions, Messages));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextProcessingExample, "TextProcessing.Example.StreamedReplyToSubtitles", TestFlags)
bool FTextProcessingExample::RunTest(const FString& Parameters)
{
	// Example: a reply arrives in network-sized pieces; each completed sentence becomes a subtitle
	// line as soon as it is known, and the remainder is shown when the stream ends.
	const TArray<FString> Pieces = {TEXT("The gate opens at 7.30 to"), TEXT("night. Bring a la"), TEXT("ntern! Thanks")};
	FTextProcessingFragmenter Fragmenter;
	TArray<FString> Subtitles;
	for (const FString& Piece : Pieces)
	{
		TArray<FString> Completed;
		if (Fragmenter.Append(Piece, Completed) != ETextProcessingResult::Ok)
		{
			AddError(TEXT("append failed"));
			return false;
		}
		for (const FString& Sentence : Completed)
		{
			Subtitles.Add(Sentence.TrimStart());
		}
	}
	FString Tail;
	Fragmenter.Flush(Tail);
	if (!Tail.IsEmpty())
	{
		Subtitles.Add(Tail.TrimStart());
	}
	TestEqual(TEXT("subtitle lines"), Join(Subtitles), FString(TEXT("The gate opens at 7.30 tonight.|Bring a lantern!|Thanks")));
	FString Label;
	TestTrue(TEXT("markup is stripped"), UTextProcessingLibrary::ReplaceRegexMatches(TEXT("<b>Gold</b> chest"), TEXT("<[^>]*>"), TEXT(""), Label));
	TestEqual(TEXT("the cleaned label"), Label, FString(TEXT("Gold chest")));
	return true;
}
