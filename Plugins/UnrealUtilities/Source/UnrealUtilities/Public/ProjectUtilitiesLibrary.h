#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ProjectUtilitiesLibrary.generated.h"

/** Project metadata helpers. */
UCLASS()
class UNREALUTILITIES_API UProjectUtilitiesLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The ProjectVersion field of the project's general settings. */
	UFUNCTION(BlueprintPure, Category = "Unreal Utilities|Project")
	static FString GetProjectVersionString();
};
