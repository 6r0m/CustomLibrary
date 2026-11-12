#pragma once

#include "CoreMinimal.h"
#include "AnimationDataTypes.h"

class UPoseAsset;

/** Turns source clips into target tracks. */
struct ANIMATIONDATATOOLS_API FAnimationDataMapper
{
	/**
	 * Validates Clip and Profile, then maps them. Pure: it reads no UObject state and runs on any
	 * thread. Returns InvalidInput with a reason in OutError for a non-positive or non-finite frame
	 * rate, no frames, curves or tracks of different lengths, non-finite values, rotations that are
	 * not unit quaternions, duplicate names (also after bone renaming), unnamed rules, or a limit
	 * whose Min exceeds its Max, or arithmetic that produces a non-finite contribution or sum.
	 */
	static EAnimationDataResult Map(const FAnimationDataClip& Clip, const FAnimationDataProfile& Profile, FAnimationDataMappedClip& OutClip, FString& OutError);

	/**
	 * Converts a pose asset's curve values into rules: one rule per pose and non-zero curve value,
	 * with the pose name as Source, the curve as Target and the value as Multiplier. Bone poses are
	 * not converted. Game thread only.
	 */
	static EAnimationDataResult MakeRulesFromPoseAsset(const UPoseAsset* PoseAsset, TArray<FAnimationDataRule>& OutRules, FString& OutError);
};
