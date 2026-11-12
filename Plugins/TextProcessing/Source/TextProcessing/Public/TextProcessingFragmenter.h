#pragma once

#include "CoreMinimal.h"
#include "TextProcessingFragmenter.generated.h"

/** Result of a fragmenter operation. A call that does not return Ok changes nothing. */
UENUM(BlueprintType)
enum class ETextProcessingResult : uint8
{
	Ok,
	/** The pending text would exceed MaxPendingChars. */
	CapExceeded,
	/** The call came from a thread other than the one that created the fragmenter. */
	WrongThread,
	/** The settings have no delimiters, a delimiter outside the Basic Multilingual Plane, or a non-positive cap. */
	InvalidSettings,
};

USTRUCT(BlueprintType)
struct TEXTPROCESSING_API FTextProcessingSettings
{
	GENERATED_BODY()

	/** Every character of this string ends a fragment. Characters outside the Basic Multilingual Plane are not allowed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text Processing|Fragments")
	FString Delimiters = TEXT(".!?;。！？");

	/** Upper bound on text held while waiting for a delimiter, in UTF-16 code units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text Processing|Fragments")
	int32 MaxPendingChars = 64 * 1024;
};

/**
 * Splits incrementally arriving text into fragments that end at a delimiter.
 *
 * Fragments are exact slices of the input, so concatenating every emitted fragment and the flushed
 * tail reproduces the input for any way it was split into deltas. Whitespace after a delimiter
 * starts the next fragment. A run of consecutive delimiters ends one fragment. A delimiter with a
 * digit on both sides (3.14, 1,000 when ',' is a delimiter) does not end a fragment. A delimiter
 * run at the end of the pending text waits for the next delta or Flush, which is what makes the
 * result independent of how the text was split.
 *
 * The fragmenter belongs to the thread that created it; calls from other threads return
 * WrongThread.
 */
class TEXTPROCESSING_API FTextProcessingFragmenter
{
public:
	explicit FTextProcessingFragmenter(const FTextProcessingSettings& InSettings = FTextProcessingSettings());

	/** Adds Delta and returns, in OutFragments, every fragment it completed. */
	ETextProcessingResult Append(FStringView Delta, TArray<FString>& OutFragments);

	/** Returns the remaining text (possibly empty) and ends the stream. The next Append starts a new one. */
	ETextProcessingResult Flush(FString& OutTail);

	/** Discards the pending text without emitting it. */
	ETextProcessingResult Reset();

	int32 GetPendingLength() const { return Pending.Len(); }

private:
	bool IsDelimiter(TCHAR Character) const;
	void EmitCompleteFragments(TArray<FString>& OutFragments);
	ETextProcessingResult CheckCaller() const;

	FTextProcessingSettings Settings;
	FString Pending;
	// Index in Pending from which boundaries are still undecided.
	int32 ScanFrom = 0;
	uint32 OwnerThreadId = 0;
	bool bValidSettings = false;
};
