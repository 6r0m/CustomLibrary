#include "AnimationDataMapper.h"
#include "Animation/PoseAsset.h"

namespace
{
	// Rotations further than this from unit length are treated as malformed input, not normalised.
	constexpr double UnitQuatTolerance = 1e-3;

	bool Fail(FString& OutError, const FString& Reason)
	{
		OutError = Reason;
		return false;
	}

	bool ValidateClip(const FAnimationDataClip& Clip, int32& OutNumFrames, FString& OutError)
	{
		if (!FMath::IsFinite(Clip.FrameRate) || Clip.FrameRate <= 0.0f)
		{
			return Fail(OutError, FString::Printf(TEXT("FrameRate must be positive and finite, got %f"), Clip.FrameRate));
		}
		OutNumFrames = !Clip.Curves.IsEmpty() ? Clip.Curves[0].Values.Num() : (!Clip.Bones.IsEmpty() ? Clip.Bones[0].Deltas.Num() : 0);
		if (OutNumFrames == 0)
		{
			return Fail(OutError, TEXT("the clip has no frames"));
		}
		TSet<FName> Names;
		for (const FAnimationDataCurve& Curve : Clip.Curves)
		{
			if (Curve.Name.IsNone() || Names.Contains(Curve.Name))
			{
				return Fail(OutError, FString::Printf(TEXT("curve name '%s' is empty or duplicated"), *Curve.Name.ToString()));
			}
			Names.Add(Curve.Name);
			if (Curve.Values.Num() != OutNumFrames)
			{
				return Fail(OutError, FString::Printf(TEXT("curve '%s' has %d frames, expected %d"), *Curve.Name.ToString(), Curve.Values.Num(), OutNumFrames));
			}
			for (const float Value : Curve.Values)
			{
				if (!FMath::IsFinite(Value))
				{
					return Fail(OutError, FString::Printf(TEXT("curve '%s' has a non-finite value"), *Curve.Name.ToString()));
				}
			}
		}
		TSet<FName> Bones;
		for (const FAnimationDataBoneTrack& Track : Clip.Bones)
		{
			if (Track.Bone.IsNone() || Bones.Contains(Track.Bone))
			{
				return Fail(OutError, FString::Printf(TEXT("bone name '%s' is empty or duplicated"), *Track.Bone.ToString()));
			}
			Bones.Add(Track.Bone);
			if (Track.Deltas.Num() != OutNumFrames)
			{
				return Fail(OutError, FString::Printf(TEXT("bone '%s' has %d frames, expected %d"), *Track.Bone.ToString(), Track.Deltas.Num(), OutNumFrames));
			}
			for (const FQuat& Delta : Track.Deltas)
			{
				if (Delta.ContainsNaN() || FMath::Abs(Delta.Size() - 1.0) > UnitQuatTolerance)
				{
					return Fail(OutError, FString::Printf(TEXT("bone '%s' has a rotation that is not a unit quaternion"), *Track.Bone.ToString()));
				}
			}
		}
		return true;
	}

	bool ValidateProfile(const FAnimationDataProfile& Profile, FString& OutError)
	{
		for (const FAnimationDataRule& Rule : Profile.Rules)
		{
			if (Rule.Source.IsNone() || Rule.Target.IsNone() || !FMath::IsFinite(Rule.Multiplier) || !FMath::IsFinite(Rule.Offset))
			{
				return Fail(OutError, FString::Printf(TEXT("rule '%s' -> '%s' needs both names and finite numbers"), *Rule.Source.ToString(), *Rule.Target.ToString()));
			}
		}
		TSet<FName> Limited;
		for (const FAnimationDataLimit& Limit : Profile.Limits)
		{
			if (Limit.Target.IsNone() || Limited.Contains(Limit.Target) || !FMath::IsFinite(Limit.Min) || !FMath::IsFinite(Limit.Max) || Limit.Min > Limit.Max)
			{
				return Fail(OutError, FString::Printf(TEXT("limit on '%s' is unnamed, duplicated, non-finite, or has Min above Max"), *Limit.Target.ToString()));
			}
			Limited.Add(Limit.Target);
		}
		for (const TPair<FName, FName>& Rename : Profile.BoneRenames)
		{
			if (Rename.Key.IsNone() || Rename.Value.IsNone())
			{
				return Fail(OutError, TEXT("bone renames need both names"));
			}
		}
		return true;
	}
}

EAnimationDataResult FAnimationDataMapper::Map(const FAnimationDataClip& Clip, const FAnimationDataProfile& Profile, FAnimationDataMappedClip& OutClip, FString& OutError)
{
	OutClip = FAnimationDataMappedClip();
	OutError.Reset();
	int32 NumFrames = 0;
	if (!ValidateClip(Clip, NumFrames, OutError) || !ValidateProfile(Profile, OutError))
	{
		return EAnimationDataResult::InvalidInput;
	}

	FAnimationDataMappedClip Result;
	Result.FrameRate = Clip.FrameRate;
	Result.NumFrames = NumFrames;

	TMap<FName, int32> SourceIndex;
	for (int32 Index = 0; Index < Clip.Curves.Num(); ++Index)
	{
		SourceIndex.Add(Clip.Curves[Index].Name, Index);
	}
	TMap<FName, int32> TargetIndex;
	auto TargetValues = [&Result, &TargetIndex, NumFrames](const FName Target) -> TArray<float>&
	{
		if (const int32* Found = TargetIndex.Find(Target))
		{
			return Result.Curves[*Found].Values;
		}
		TargetIndex.Add(Target, Result.Curves.Num());
		FAnimationDataCurve& Curve = Result.Curves.AddDefaulted_GetRef();
		Curve.Name = Target;
		// Every target is keyed at every frame, including frames where it is zero.
		Curve.Values.SetNumZeroed(NumFrames);
		return Curve.Values;
	};

	TSet<FName> ReadSources;
	for (const FAnimationDataRule& Rule : Profile.Rules)
	{
		ReadSources.Add(Rule.Source);
		const int32* Source = SourceIndex.Find(Rule.Source);
		if (!Source)
		{
			Result.MissingSources.AddUnique(Rule.Source);
			continue;
		}
		const TArray<float>& Values = Clip.Curves[*Source].Values;
		TArray<float>& Target = TargetValues(Rule.Target);
		for (int32 Frame = 0; Frame < NumFrames; ++Frame)
		{
			const float Contribution = Values[Frame] * Rule.Multiplier + Rule.Offset;
			if (!FMath::IsFinite(Contribution))
			{
				OutError = FString::Printf(TEXT("target '%s' frame %d overflowed while applying rule '%s'"), *Rule.Target.ToString(), Frame, *Rule.Source.ToString());
				return EAnimationDataResult::InvalidInput;
			}
			const float Accumulated = Target[Frame] + Contribution;
			if (!FMath::IsFinite(Accumulated))
			{
				OutError = FString::Printf(TEXT("target '%s' frame %d overflowed while accumulating"), *Rule.Target.ToString(), Frame);
				return EAnimationDataResult::InvalidInput;
			}
			Target[Frame] = Accumulated;
		}
	}
	if (Profile.bPassThroughUnmapped)
	{
		for (const FAnimationDataCurve& Curve : Clip.Curves)
		{
			if (!ReadSources.Contains(Curve.Name))
			{
				TArray<float>& Target = TargetValues(Curve.Name);
				for (int32 Frame = 0; Frame < NumFrames; ++Frame)
				{
					const float Accumulated = Target[Frame] + Curve.Values[Frame];
					if (!FMath::IsFinite(Accumulated))
					{
						OutError = FString::Printf(TEXT("target '%s' frame %d overflowed while passing through '%s'"), *Curve.Name.ToString(), Frame, *Curve.Name.ToString());
						return EAnimationDataResult::InvalidInput;
					}
					Target[Frame] = Accumulated;
				}
			}
		}
	}
	for (const FAnimationDataLimit& Limit : Profile.Limits)
	{
		if (const int32* Found = TargetIndex.Find(Limit.Target))
		{
			for (float& Value : Result.Curves[*Found].Values)
			{
				Value = FMath::Clamp(Value, Limit.Min, Limit.Max);
			}
		}
	}

	TSet<FName> TargetBones;
	for (const FAnimationDataBoneTrack& Track : Clip.Bones)
	{
		const FName* Renamed = Profile.BoneRenames.Find(Track.Bone);
		FAnimationDataBoneTrack Mapped;
		Mapped.Bone = Renamed ? *Renamed : Track.Bone;
		if (TargetBones.Contains(Mapped.Bone))
		{
			OutError = FString::Printf(TEXT("two bones map to '%s'"), *Mapped.Bone.ToString());
			return EAnimationDataResult::InvalidInput;
		}
		TargetBones.Add(Mapped.Bone);
		Mapped.Deltas = Track.Deltas;
		Result.Bones.Add(MoveTemp(Mapped));
	}

	OutClip = MoveTemp(Result);
	return EAnimationDataResult::Ok;
}

EAnimationDataResult FAnimationDataMapper::MakeRulesFromPoseAsset(const UPoseAsset* PoseAsset, TArray<FAnimationDataRule>& OutRules, FString& OutError)
{
	OutRules.Reset();
	OutError.Reset();
	if (!IsInGameThread())
	{
		OutError = TEXT("pose assets are read on the game thread");
		return EAnimationDataResult::WrongThread;
	}
	if (!PoseAsset)
	{
		OutError = TEXT("no pose asset");
		return EAnimationDataResult::InvalidInput;
	}
	const TArray<FName>& Poses = PoseAsset->GetPoseFNames();
	const TArray<FName> Curves = PoseAsset->GetCurveFNames();
	for (int32 PoseIndex = 0; PoseIndex < Poses.Num(); ++PoseIndex)
	{
		const TArray<float>& Values = PoseAsset->GetCurveValues(PoseIndex);
		for (int32 CurveIndex = 0; CurveIndex < FMath::Min(Curves.Num(), Values.Num()); ++CurveIndex)
		{
			if (Values[CurveIndex] != 0.0f)
			{
				FAnimationDataRule& Rule = OutRules.AddDefaulted_GetRef();
				Rule.Source = Poses[PoseIndex];
				Rule.Target = Curves[CurveIndex];
				Rule.Multiplier = Values[CurveIndex];
			}
		}
	}
	return EAnimationDataResult::Ok;
}
