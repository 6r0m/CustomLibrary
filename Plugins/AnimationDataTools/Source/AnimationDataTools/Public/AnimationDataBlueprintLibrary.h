#pragma once

#include "CoreMinimal.h"
#include "AnimationDataTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnimationDataBlueprintLibrary.generated.h"

class UPoseAsset;

/** Blueprint access to FAnimationDataMapper. */
UCLASS()
class ANIMATIONDATATOOLS_API UAnimationDataBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Animation Data Tools")
	static EAnimationDataResult MapClip(const FAnimationDataClip& Clip, const FAnimationDataProfile& Profile, FAnimationDataMappedClip& OutClip, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Animation Data Tools")
	static EAnimationDataResult MakeRulesFromPoseAsset(const UPoseAsset* PoseAsset, TArray<FAnimationDataRule>& OutRules, FString& OutError);
};
