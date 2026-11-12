#pragma once

#include "CoreMinimal.h"

// Case data for the TextProcessing contract tests. Each case lists the deltas fed in order, the
// fragments Append must emit across all of them, and the tail Flush must return.
namespace TextCases
{
	struct FStreamCase
	{
		const TCHAR* Name;
		TArray<FString> Deltas;
		TArray<FString> Fragments;
		FString Tail;
	};

	inline TArray<FStreamCase> StreamCases()
	{
		return {
			// X1: a repeated fragment is emitted every time it occurs.
			{TEXT("X1 repeat"), {TEXT("No."), TEXT("No."), TEXT("No.")}, {TEXT("No."), TEXT("No.")}, TEXT("No.")},
			// X2: a delimiter between digits is part of a number.
			{TEXT("X2 decimal"), {TEXT("Pi is 3.14 today.")}, {}, TEXT("Pi is 3.14 today.")},
			{TEXT("X2 decimal, then more"), {TEXT("Pi is 3.14 today. Yes")}, {TEXT("Pi is 3.14 today.")}, TEXT(" Yes")},
			// X3: a delimiter that arrives alone is decided by the next delta.
			{TEXT("X3 split punctuation"), {TEXT("Hi"), TEXT("."), TEXT(" There.")}, {TEXT("Hi.")}, TEXT(" There.")},
			{TEXT("X3 split decimal"), {TEXT("Costs 3"), TEXT("."), TEXT("50 now. Ok")}, {TEXT("Costs 3.50 now.")}, TEXT(" Ok")},
			// X4: text after the last delimiter is returned by Flush.
			{TEXT("X4 tail"), {TEXT("Hello. How are you")}, {TEXT("Hello.")}, TEXT(" How are you")},
			// A run of delimiters ends one fragment.
			{TEXT("delimiter run"), {TEXT("Wait..."), TEXT("?! Go")}, {TEXT("Wait...?!")}, TEXT(" Go")},
			// X6: empty deltas emit nothing.
			{TEXT("X6 empty"), {TEXT(""), TEXT(""), TEXT("A."), TEXT(""), TEXT(" B")}, {TEXT("A.")}, TEXT(" B")},
			// X8: CJK delimiters, a combining mark before a delimiter, and non-BMP text, including a
			// surrogate pair split across deltas.
			{TEXT("X8 CJK"), {TEXT("你好。世界！")}, {TEXT("你好。")}, TEXT("世界！")},
			{TEXT("X8 combining"), {TEXT("Café. Ok")}, {TEXT("Café.")}, TEXT(" Ok")},
			{TEXT("X8 non-BMP"), {TEXT("\xD83D"), TEXT("\xDE00! x")}, {TEXT("\xD83D\xDE00!")}, TEXT(" x")},
		};
	}

	// X9: text for the partition test. It mixes every boundary rule.
	inline FString PartitionText()
	{
		return TEXT("Pi is 3.14 today. Wow!! Really?! 你好。世界！ Done; ok... Café. \xD83D\xDE00! 1,000.5 units. End");
	}

	constexpr int32 PartitionRuns = 1000;
	constexpr int32 PartitionSeed = 20261109;
	constexpr int32 MaxPieceLength = 7;
}
