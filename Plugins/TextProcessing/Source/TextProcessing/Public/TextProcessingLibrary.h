#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TextProcessingLibrary.generated.h"

/** Text operations shared by C++ and Blueprint consumers. */
UCLASS()
class TEXTPROCESSING_API UTextProcessingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Replaces every match of Pattern (ICU regular expression syntax) in Input with Replacement, which
	 * is inserted literally: "$1" and backslashes are not interpreted. Zero-length matches terminate.
	 * Returns false and sets OutText to Input when Pattern does not compile. This is not an HTML
	 * sanitizer.
	 */
	UFUNCTION(BlueprintCallable, Category = "Text Processing|Regex")
	static bool ReplaceRegexMatches(const FString& Input, const FString& Pattern, const FString& Replacement, FString& OutText);
};
