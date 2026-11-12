#pragma once

#include "AnimationDataTypes.h"
#include "CoreMinimal.h"

// Case data for the AnimationDataTools mapper tests.
namespace MappingCases
{
	inline FAnimationDataCurve MakeCurve(const TCHAR* Name, TArray<float> Values)
	{
		FAnimationDataCurve Result;
		Result.Name = Name;
		Result.Values = MoveTemp(Values);
		return Result;
	}

	inline FAnimationDataRule MakeRule(const TCHAR* Source, const TCHAR* Target, float Multiplier = 1.0f, float Offset = 0.0f)
	{
		FAnimationDataRule Result;
		Result.Source = Source;
		Result.Target = Target;
		Result.Multiplier = Multiplier;
		Result.Offset = Offset;
		return Result;
	}

	inline FAnimationDataClip MakeClip(TArray<FAnimationDataCurve> Curves, float FrameRate = 30.0f)
	{
		FAnimationDataClip Result;
		Result.FrameRate = FrameRate;
		Result.Curves = MoveTemp(Curves);
		return Result;
	}

	inline const FAnimationDataCurve* Find(const FAnimationDataMappedClip& Mapped, const TCHAR* Name)
	{
		return Mapped.Curves.FindByPredicate([Name](const FAnimationDataCurve& Curve) { return Curve.Name == FName(Name); });
	}

	constexpr int32 WideChannels = 300;
	constexpr int32 PermutationSeed = 7;
	constexpr float Tolerance = 1e-6f;
	/** Engine skeleton with two bones, shipped in Engine content. */
	inline const TCHAR* SkeletonPath() { return TEXT("/Engine/EngineMeshes/SkeletalCube_Skeleton.SkeletalCube_Skeleton"); }
}
