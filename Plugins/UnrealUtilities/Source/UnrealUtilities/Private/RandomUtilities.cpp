#include "RandomUtilitiesLibrary.h"
#include "UnrealUtilitiesLog.h"
#include "Math/RandomStream.h"

namespace
{
	// Validates the weights and returns their total, or a negative value when they are unusable.
	int64 ValidatedTotal(const TArray<int32>& Weights)
	{
		if (Weights.IsEmpty())
		{
			return -1;
		}
		int64 Total = 0;
		for (const int32 Weight : Weights)
		{
			if (Weight < 0)
			{
				return -1;
			}
			Total += Weight;
		}
		// The draw uses int32 arithmetic, so the total must fit in it.
		return (Total > 0 && Total <= MAX_int32) ? Total : -1;
	}

	int32 IndexForDraw(const TArray<int32>& Weights, const int32 Draw)
	{
		int64 Cumulative = 0;
		for (int32 Index = 0; Index < Weights.Num(); ++Index)
		{
			Cumulative += Weights[Index];
			if (Draw < Cumulative)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	template <typename DrawFunction>
	bool SelectWeightedIndex(const TArray<int32>& Weights, int32& OutIndex, DrawFunction Draw)
	{
		OutIndex = INDEX_NONE;
		const int64 Total = ValidatedTotal(Weights);
		if (Total < 0)
		{
			UE_LOG(LogUnrealUtilities, Warning, TEXT("Weighted selection refused: weights must be non-negative, not all zero, and total at most %d."), MAX_int32);
			return false;
		}
		// The draw is uniform over [0, Total - 1], so each index owns exactly Weight values.
		OutIndex = IndexForDraw(Weights, Draw(static_cast<int32>(Total) - 1));
		return OutIndex != INDEX_NONE;
	}
}

bool URandomUtilitiesLibrary::TrySelectWeightedIndex(const TArray<int32>& Weights, int32& OutIndex)
{
	return SelectWeightedIndex(Weights, OutIndex, [](const int32 Max) { return FMath::RandRange(0, Max); });
}

bool URandomUtilitiesLibrary::TrySelectWeightedIndexFromStream(const TArray<int32>& Weights, FRandomStream& Stream, int32& OutIndex)
{
	return SelectWeightedIndex(Weights, OutIndex, [&Stream](const int32 Max) { return Stream.RandRange(0, Max); });
}
