#include "AnimationDataSequenceWriter.h"

#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimData/CurveIdentifier.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimSequence.h"
#include "Curves/RichCurve.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#define LOCTEXT_NAMESPACE "AnimationDataSequenceWriter"

namespace
{
	constexpr double UnitQuatTolerance = 1e-3;

	bool Fail(FString& OutError, const FString& Reason)
	{
		OutError = Reason;
		return false;
	}

	// The animation data controller rejects a frame rate whose interval is near zero. Its tolerance
	// is FMath::IsNearlyZero's, about 1e-4 seconds (10,000 fps).
	bool CheckControllerRate(const FFrameRate& Rate, FString& OutError)
	{
		const double Interval = Rate.AsInterval();
		if (!Rate.IsValid() || Rate.Numerator <= 0 || !(Interval > 0.0) || FMath::IsNearlyZero(Interval))
		{
			return Fail(OutError, FString::Printf(TEXT("FrameRate %f has an interval the animation data controller rejects"), Rate.AsDecimal()));
		}
		return true;
	}

	// Converts a channel rate to a rate the engine can represent without a zero or overflowing
	// numerator, and one whose interval the animation data controller accepts. Rates below 0.001 fps,
	// rates whose numerator would not fit an int32, and rates near the controller's interval tolerance
	// are refused.
	bool TryMakeFrameRate(const float Rate, FFrameRate& OutRate, FString& OutError)
	{
		const double RateDouble = static_cast<double>(Rate);
		if (!FMath::IsFinite(Rate) || !(RateDouble > 0.0))
		{
			return Fail(OutError, FString::Printf(TEXT("FrameRate must be positive and finite, got %f"), Rate));
		}
		if (RateDouble >= 2147483648.0)
		{
			return Fail(OutError, FString::Printf(TEXT("FrameRate %f is too large to represent"), Rate));
		}
		const int32 Whole = FMath::RoundToInt32(Rate);
		if (FMath::IsNearlyEqual(Rate, static_cast<float>(Whole), 1e-4f))
		{
			if (Whole <= 0)
			{
				return Fail(OutError, FString::Printf(TEXT("FrameRate %f is too small to represent"), Rate));
			}
			OutRate = FFrameRate(Whole, 1);
			return CheckControllerRate(OutRate, OutError);
		}
		const double Thousandths = RateDouble * 1000.0;
		if (Thousandths < 1.0 || Thousandths >= 2147483648.0)
		{
			return Fail(OutError, FString::Printf(TEXT("FrameRate %f is outside the representable range"), Rate));
		}
		const int32 Numerator = FMath::RoundToInt32(Thousandths);
		if (Numerator <= 0)
		{
			return Fail(OutError, FString::Printf(TEXT("FrameRate %f is too small to represent"), Rate));
		}
		OutRate = FFrameRate(Numerator, 1000);
		return CheckControllerRate(OutRate, OutError);
	}

	bool Validate(const FAnimationDataMappedClip& Clip, const FAnimationDataBuildOptions& Options, FFrameRate& OutRate, FString& OutError)
	{
		if (!Options.Skeleton)
		{
			return Fail(OutError, TEXT("no skeleton"));
		}
		if (!TryMakeFrameRate(Clip.FrameRate, OutRate, OutError))
		{
			return false;
		}
		if (Clip.NumFrames < 1)
		{
			return Fail(OutError, FString::Printf(TEXT("need at least one frame, got %d"), Clip.NumFrames));
		}
		TSet<FName> Names;
		for (const FAnimationDataCurve& Curve : Clip.Curves)
		{
			if (Curve.Name.IsNone() || Names.Contains(Curve.Name) || Curve.Values.Num() != Clip.NumFrames)
			{
				return Fail(OutError, FString::Printf(TEXT("curve '%s' is unnamed, duplicated, or not %d frames long"), *Curve.Name.ToString(), Clip.NumFrames));
			}
			Names.Add(Curve.Name);
			for (const float Value : Curve.Values)
			{
				if (!FMath::IsFinite(Value))
				{
					return Fail(OutError, FString::Printf(TEXT("curve '%s' has a non-finite value"), *Curve.Name.ToString()));
				}
			}
		}
		const FReferenceSkeleton& Reference = Options.Skeleton->GetReferenceSkeleton();
		TSet<FName> Bones;
		for (const FAnimationDataBoneTrack& Track : Clip.Bones)
		{
			if (Track.Bone.IsNone() || Bones.Contains(Track.Bone) || Track.Deltas.Num() != Clip.NumFrames)
			{
				return Fail(OutError, FString::Printf(TEXT("bone '%s' is unnamed, duplicated, or not %d frames long"), *Track.Bone.ToString(), Clip.NumFrames));
			}
			Bones.Add(Track.Bone);
			if (Reference.FindBoneIndex(Track.Bone) == INDEX_NONE)
			{
				return Fail(OutError, FString::Printf(TEXT("bone '%s' is not in skeleton %s"), *Track.Bone.ToString(), *Options.Skeleton->GetName()));
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

	void Populate(UAnimSequence* Sequence, const FAnimationDataMappedClip& Clip, const FAnimationDataBuildOptions& Options, const FFrameRate& Rate, FAnimationDataBuildReport& OutReport)
	{
		for (const FAnimationDataCurve& Curve : Clip.Curves)
		{
			if (!Options.Skeleton->GetCurveMetaData(Curve.Name))
			{
				OutReport.CurvesMissingFromSkeleton.Add(Curve.Name);
			}
		}
		OutReport.bMissingCurvesDropped = Options.bDropCurvesMissingFromSkeleton && !OutReport.CurvesMissingFromSkeleton.IsEmpty();

		Sequence->SetSkeleton(Options.Skeleton);
		Sequence->Interpolation = Options.Interpolation;
		const int32 Frames = Clip.NumFrames;
		IAnimationDataController& Controller = Sequence->GetController();
		Controller.InitializeModel();
		Controller.OpenBracket(LOCTEXT("Write", "Write channel animation"), false);
		Controller.SetFrameRate(Rate, false);
		Controller.SetNumberOfFrames(FFrameNumber(Frames), false);

		const FReferenceSkeleton& Reference = Options.Skeleton->GetReferenceSkeleton();
		for (const FAnimationDataBoneTrack& Track : Clip.Bones)
		{
			const FTransform& RefPose = Reference.GetRefBonePose()[Reference.FindBoneIndex(Track.Bone)];
			TArray<FVector> Positions;
			TArray<FQuat> Rotations;
			TArray<FVector> Scales;
			for (int32 Key = 0; Key <= Frames; ++Key)
			{
				// Key N repeats frame N - 1, so the last frame holds for its whole interval.
				const FQuat& Delta = Track.Deltas[FMath::Min(Key, Frames - 1)];
				Positions.Add(RefPose.GetTranslation());
				Rotations.Add((RefPose.GetRotation() * Delta).GetNormalized());
				Scales.Add(RefPose.GetScale3D());
			}
			Controller.AddBoneCurve(Track.Bone, false);
			Controller.SetBoneTrackKeys(Track.Bone, Positions, Rotations, Scales, false);
		}

		const ERichCurveInterpMode InterpMode = Options.Interpolation == EAnimInterpolationType::Step ? RCIM_Constant : RCIM_Linear;
		for (const FAnimationDataCurve& Curve : Clip.Curves)
		{
			if (OutReport.bMissingCurvesDropped && OutReport.CurvesMissingFromSkeleton.Contains(Curve.Name))
			{
				continue;
			}
			const FAnimationCurveIdentifier Id(Curve.Name, ERawCurveTrackTypes::RCT_Float);
			Controller.AddCurve(Id, AACF_DefaultCurve, false);
			TArray<FRichCurveKey> Keys;
			for (int32 Key = 0; Key <= Frames; ++Key)
			{
				FRichCurveKey& RichKey = Keys.Emplace_GetRef(static_cast<float>(Rate.AsSeconds(FFrameNumber(Key))), Curve.Values[FMath::Min(Key, Frames - 1)]);
				RichKey.InterpMode = InterpMode;
			}
			Controller.SetCurveKeys(Id, Keys, false);
		}
		Controller.NotifyPopulated();
		Controller.CloseBracket(false);
	}
}

EAnimationDataResult FAnimationDataSequenceWriter::WriteTransient(const FAnimationDataMappedClip& Clip, const FAnimationDataBuildOptions& Options,
	UAnimSequence*& OutSequence, FAnimationDataBuildReport& OutReport, FString& OutError)
{
	OutSequence = nullptr;
	OutReport = FAnimationDataBuildReport();
	OutError.Reset();
	if (!IsInGameThread())
	{
		OutError = TEXT("sequences are written on the game thread");
		return EAnimationDataResult::WrongThread;
	}
	FFrameRate Rate;
	if (!Validate(Clip, Options, Rate, OutError))
	{
		return EAnimationDataResult::InvalidInput;
	}
	UPackage* Package = GetTransientPackage();
	UAnimSequence* Sequence = NewObject<UAnimSequence>(Package, MakeUniqueObjectName(Package, UAnimSequence::StaticClass(), TEXT("AnimationDataTools")), RF_Transient);
	Populate(Sequence, Clip, Options, Rate, OutReport);
	OutSequence = Sequence;
	return EAnimationDataResult::Ok;
}

EAnimationDataResult FAnimationDataSequenceWriter::WriteAsset(const FAnimationDataMappedClip& Clip, const FAnimationDataBuildOptions& Options,
	const FString& PackageName, UAnimSequence*& OutSequence, FAnimationDataBuildReport& OutReport, FString& OutError)
{
	OutSequence = nullptr;
	OutReport = FAnimationDataBuildReport();
	OutError.Reset();
	if (!IsInGameThread())
	{
		OutError = TEXT("assets are written on the game thread");
		return EAnimationDataResult::WrongThread;
	}
	FText Reason;
	if (!FPackageName::IsValidLongPackageName(PackageName, false, &Reason))
	{
		OutError = Reason.ToString();
		return EAnimationDataResult::InvalidInput;
	}
	if (FindPackage(nullptr, *PackageName) || FPackageName::DoesPackageExist(PackageName))
	{
		OutError = FString::Printf(TEXT("an asset already exists at %s"), *PackageName);
		return EAnimationDataResult::InvalidInput;
	}
	FFrameRate Rate;
	if (!Validate(Clip, Options, Rate, OutError))
	{
		return EAnimationDataResult::InvalidInput;
	}

	UPackage* Package = CreatePackage(*PackageName);
	UAnimSequence* Sequence = NewObject<UAnimSequence>(Package, *FPackageName::GetLongPackageAssetName(PackageName), RF_Public | RF_Standalone);
	Populate(Sequence, Clip, Options, Rate, OutReport);

	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, Sequence, *Filename, SaveArgs))
	{
		Sequence->ClearFlags(RF_Public | RF_Standalone);
		Sequence->MarkAsGarbage();
		Package->MarkAsGarbage();
		OutError = FString::Printf(TEXT("could not save %s"), *Filename);
		return EAnimationDataResult::SaveFailed;
	}
	FAssetRegistryModule::AssetCreated(Sequence);
	OutSequence = Sequence;
	return EAnimationDataResult::Ok;
}

#undef LOCTEXT_NAMESPACE
