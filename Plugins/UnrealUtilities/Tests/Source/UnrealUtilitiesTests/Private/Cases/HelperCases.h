#pragma once

#include "CoreMinimal.h"

// Case data for the helper tests. Pure data, so any harness can run the same cases.
namespace HelperCases
{
	// Weighted selection. Valid cases must never pick a zero-weight index and must follow the weights.
	struct FWeightedCase
	{
		const TCHAR* Name;
		TArray<int32> Weights;
		bool bValid;
	};

	inline TArray<FWeightedCase> WeightedCases()
	{
		return {
			{ TEXT("ZeroWeightFirst"), { 0, 1 }, true },
			{ TEXT("EqualPair"), { 1, 1 }, true },
			{ TEXT("Empty"), {}, false },
			{ TEXT("Negative"), { -1, 2 }, false },
			{ TEXT("SumOverflow"), { MAX_int32, 1 }, false },
			{ TEXT("AllZero"), { 0, 0 }, false },
		};
	}

	constexpr int32 WeightedDraws = 20000;
	constexpr int32 WeightedSeed = 1234;
	// Allowed deviation of index 0's share from 0.5 for the equal pair.
	constexpr double EqualPairTolerance = 0.03;

	inline const TCHAR* ProjectVersionForTest()
	{
		return TEXT("9.8.7-test");
	}
}
