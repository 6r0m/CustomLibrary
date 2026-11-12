#include "AnimationDataBlueprintLibrary.h"
#include "AnimationDataMapper.h"

EAnimationDataResult UAnimationDataBlueprintLibrary::MapClip(const FAnimationDataClip& Clip, const FAnimationDataProfile& Profile, FAnimationDataMappedClip& OutClip, FString& OutError)
{
	return FAnimationDataMapper::Map(Clip, Profile, OutClip, OutError);
}

EAnimationDataResult UAnimationDataBlueprintLibrary::MakeRulesFromPoseAsset(const UPoseAsset* PoseAsset, TArray<FAnimationDataRule>& OutRules, FString& OutError)
{
	return FAnimationDataMapper::MakeRulesFromPoseAsset(PoseAsset, OutRules, OutError);
}
