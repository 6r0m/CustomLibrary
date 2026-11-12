#pragma once

#include "CoreMinimal.h"
#include "TextProcessingFragmenter.h"
#include "UObject/Object.h"
#include "TextProcessingFragmenterObject.generated.h"

/** Blueprint access to FTextProcessingFragmenter. It belongs to the thread that created it. */
UCLASS(BlueprintType)
class TEXTPROCESSING_API UTextProcessingFragmenterObject : public UObject
{
	GENERATED_BODY()

public:
	/** Creates a fragmenter with the given settings. */
	UFUNCTION(BlueprintCallable, Category = "Text Processing|Fragments")
	static UTextProcessingFragmenterObject* CreateFragmenter(const FTextProcessingSettings& Settings);

	/** Adds a delta and returns the fragments it completed. */
	UFUNCTION(BlueprintCallable, Category = "Text Processing|Fragments")
	ETextProcessingResult Append(const FString& Delta, TArray<FString>& OutFragments);

	/** Returns the remaining text and ends the stream. */
	UFUNCTION(BlueprintCallable, Category = "Text Processing|Fragments")
	ETextProcessingResult Flush(FString& OutTail);

	/** Discards the pending text. */
	UFUNCTION(BlueprintCallable, Category = "Text Processing|Fragments")
	ETextProcessingResult Reset();

private:
	TUniquePtr<FTextProcessingFragmenter> Fragmenter;
};
