#include "TextProcessingFragmenter.h"
#include "HAL/PlatformTLS.h"

namespace
{
struct FScanResult
	{
		int32 Start = 0;
		int32 NextScan = 0;
	};

	template <typename ReadCharType, typename IsDelimiterType, typename OnFragmentType>
	FScanResult ScanFragments(const int32 Length, const int32 StartScan, ReadCharType Read, IsDelimiterType IsDelimiter, OnFragmentType OnFragment)
	{
		int32 Start = 0;
		int32 Index = StartScan;
		while (Index < Length)
		{
			if (!IsDelimiter(Read(Index)))
			{
				++Index;
				continue;
			}
			int32 RunEnd = Index;
			while (RunEnd + 1 < Length && IsDelimiter(Read(RunEnd + 1)))
			{
				++RunEnd;
			}
			if (RunEnd == Length - 1)
			{
				// The run touches the end: the next character decides whether it is a boundary.
				break;
			}
			const bool bBetweenDigits = RunEnd == Index && Index > 0
				&& FChar::IsDigit(Read(Index - 1)) && FChar::IsDigit(Read(Index + 1));
			if (bBetweenDigits)
			{
				Index = RunEnd + 1;
				continue;
			}
			OnFragment(Start, RunEnd + 1);
			Start = RunEnd + 1;
			Index = Start;
		}
		return FScanResult{Start, Index - Start};
	}
	bool AreValid(const FTextProcessingSettings& Settings)
	{
		if (Settings.Delimiters.IsEmpty() || Settings.MaxPendingChars <= 0)
		{
			return false;
		}
		// A delimiter is one UTF-16 code unit. Half of a surrogate pair would split a character.
		for (const TCHAR Character : Settings.Delimiters)
		{
			if (Character >= 0xD800 && Character <= 0xDFFF)
			{
				return false;
			}
		}
		return true;
	}
}

FTextProcessingFragmenter::FTextProcessingFragmenter(const FTextProcessingSettings& InSettings)
	: Settings(InSettings)
	, OwnerThreadId(FPlatformTLS::GetCurrentThreadId())
	, bValidSettings(AreValid(InSettings))
{
}

ETextProcessingResult FTextProcessingFragmenter::CheckCaller() const
{
	if (FPlatformTLS::GetCurrentThreadId() != OwnerThreadId)
	{
		return ETextProcessingResult::WrongThread;
	}
	return bValidSettings ? ETextProcessingResult::Ok : ETextProcessingResult::InvalidSettings;
}

bool FTextProcessingFragmenter::IsDelimiter(const TCHAR Character) const
{
	int32 Unused;
	return Settings.Delimiters.FindChar(Character, Unused);
}

ETextProcessingResult FTextProcessingFragmenter::Append(const FStringView Delta, TArray<FString>& OutFragments)
{
	OutFragments.Reset();
	const ETextProcessingResult Caller = CheckCaller();
	if (Caller != ETextProcessingResult::Ok)
	{
		return Caller;
	}
	if (Delta.IsEmpty())
	{
		return ETextProcessingResult::Ok;
	}
	// Preflight on input views: the retained tail, not the incoming batch, must fit. Nothing is
	// appended or copied when the tail would overflow.
	const int32 PendingLength = Pending.Len();
	const int64 TotalLength64 = static_cast<int64>(PendingLength) + Delta.Len();
	if (TotalLength64 > MAX_int32)
	{
		return ETextProcessingResult::CapExceeded;
	}
	const int32 TotalLength = static_cast<int32>(TotalLength64);
	auto Read = [this, &Delta, PendingLength](const int32 Index) -> TCHAR
	{
		return Index < PendingLength ? Pending[Index] : Delta[Index - PendingLength];
	};
	const FScanResult Prospective = ScanFragments(TotalLength, ScanFrom, Read,
		[this](const TCHAR Character) { return IsDelimiter(Character); },
		[](int32, int32) {});
	if (TotalLength - Prospective.Start > Settings.MaxPendingChars)
	{
		return ETextProcessingResult::CapExceeded;
	}
	Pending.Append(Delta);
	EmitCompleteFragments(OutFragments);
	// The batch may have been large even though the tail is small; keep only tail-sized storage.
	FString Compacted = Pending.Mid(0, Pending.Len());
	Pending = MoveTemp(Compacted);
	return ETextProcessingResult::Ok;
}

void FTextProcessingFragmenter::EmitCompleteFragments(TArray<FString>& OutFragments)
{
	const FScanResult Scan = ScanFragments(Pending.Len(), ScanFrom,
		[this](const int32 Index) { return Pending[Index]; },
		[this](const TCHAR Character) { return IsDelimiter(Character); },
		[this, &OutFragments](const int32 Begin, const int32 End)
		{
			OutFragments.Add(Pending.Mid(Begin, End - Begin));
		});
	Pending.RemoveAt(0, Scan.Start, EAllowShrinking::No);
	ScanFrom = Scan.NextScan;
}

ETextProcessingResult FTextProcessingFragmenter::Flush(FString& OutTail)
{
	OutTail.Reset();
	const ETextProcessingResult Caller = CheckCaller();
	if (Caller != ETextProcessingResult::Ok)
	{
		return Caller;
	}
	OutTail = MoveTemp(Pending);
	Pending.Reset();
	ScanFrom = 0;
	return ETextProcessingResult::Ok;
}

ETextProcessingResult FTextProcessingFragmenter::Reset()
{
	const ETextProcessingResult Caller = CheckCaller();
	if (Caller != ETextProcessingResult::Ok)
	{
		return Caller;
	}
	Pending.Reset();
	ScanFrom = 0;
	return ETextProcessingResult::Ok;
}
