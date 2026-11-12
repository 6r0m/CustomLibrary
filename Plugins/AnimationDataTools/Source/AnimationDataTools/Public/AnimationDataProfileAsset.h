#pragma once

#include "CoreMinimal.h"
#include "AnimationDataTypes.h"
#include "Engine/DataAsset.h"
#include "AnimationDataProfileAsset.generated.h"

/** A mapping profile stored as an asset, so one profile can serve many clips. */
UCLASS(BlueprintType)
class ANIMATIONDATATOOLS_API UAnimationDataProfileAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	FAnimationDataProfile Profile;
};
