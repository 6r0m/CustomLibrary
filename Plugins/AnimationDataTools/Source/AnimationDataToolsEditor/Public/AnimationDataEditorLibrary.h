#pragma once

#include "AnimationDataSequenceWriter.h"
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnimationDataEditorLibrary.generated.h"

class UAnimSequence;

/** Blueprint access to FAnimationDataSequenceWriter, for editor utilities. */
UCLASS()
class ANIMATIONDATATOOLSEDITOR_API UAnimationDataEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Animation Data Tools|Editor")
	static EAnimationDataResult WriteTransientSequence(const FAnimationDataMappedClip& Clip, const FAnimationDataBuildOptions& Options,
		UAnimSequence*& OutSequence, FAnimationDataBuildReport& OutReport, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Animation Data Tools|Editor")
	static EAnimationDataResult WriteSequenceAsset(const FAnimationDataMappedClip& Clip, const FAnimationDataBuildOptions& Options,
		const FString& PackageName, UAnimSequence*& OutSequence, FAnimationDataBuildReport& OutReport, FString& OutError);
};
