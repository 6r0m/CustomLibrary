#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RandomUtilitiesLibrary.generated.h"

/** Validated random selection helpers, usable from C++ and Blueprints. */
UCLASS()
class UNREALUTILITIES_API URandomUtilitiesLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Picks an index with probability proportional to its weight, using the global random generator.
	 * Zero-weight entries are never picked. Returns false and sets OutIndex to INDEX_NONE when the
	 * array is empty, any weight is negative, every weight is zero, or the total exceeds MAX_int32.
	 */
	UFUNCTION(BlueprintCallable, Category = "Unreal Utilities|Random")
	static bool TrySelectWeightedIndex(const TArray<int32>& Weights, int32& OutIndex);

	/** Same as TrySelectWeightedIndex, drawing from Stream so results are reproducible. */
	UFUNCTION(BlueprintCallable, Category = "Unreal Utilities|Random")
	static bool TrySelectWeightedIndexFromStream(const TArray<int32>& Weights, UPARAM(ref) FRandomStream& Stream, int32& OutIndex);
};
