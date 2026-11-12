#pragma once

#include "Animation/AnimTypes.h"
#include "AnimationDataTypes.h"
#include "CoreMinimal.h"
#include "AnimationDataSequenceWriter.generated.h"

class UAnimSequence;
class USkeleton;

USTRUCT(BlueprintType)
struct ANIMATIONDATATOOLSEDITOR_API FAnimationDataBuildOptions
{
	GENERATED_BODY()

	/** Skeleton the sequence animates. Every mapped bone must exist in it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	TObjectPtr<USkeleton> Skeleton = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	EAnimInterpolationType Interpolation = EAnimInterpolationType::Linear;

	/** Curves the skeleton has no metadata for are written unless this is set. They are listed in the report either way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	bool bDropCurvesMissingFromSkeleton = false;
};

USTRUCT(BlueprintType)
struct ANIMATIONDATATOOLSEDITOR_API FAnimationDataBuildReport
{
	GENERATED_BODY()

	/** Mapped curves the skeleton has no metadata for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	TArray<FName> CurvesMissingFromSkeleton;

	/** True when those curves were left out of the sequence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	bool bMissingCurvesDropped = false;
};

/**
 * Writes a mapped clip as an animation sequence through the engine's animation data controller.
 *
 * A clip of N frames at F frames per second becomes a sequence of length N / F with N + 1 keys:
 * key i holds frame i at time i / F, and key N repeats frame N - 1, so the last frame holds for
 * its whole interval. Bone keys are Reference * Delta in the bone's local space; bones without a
 * track keep the reference pose. The frame rate must be representable: at least 0.001 fps, with a
 * numerator that fits an int32, and with an interval the animation data controller accepts (not
 * near its zero tolerance, about 10,000 fps); otherwise the call returns InvalidInput. Game thread
 * only. Preflight rejection (InvalidInput or WrongThread) creates no object. SaveFailed is
 * different: it marks the in-memory sequence and package for garbage collection but does not promise
 * filesystem rollback; WriteAsset documents the disk contract.
 */
struct ANIMATIONDATATOOLSEDITOR_API FAnimationDataSequenceWriter
{
	/** Writes a sequence in the transient package, for previewing or further processing. */
	static EAnimationDataResult WriteTransient(const FAnimationDataMappedClip& Clip, const FAnimationDataBuildOptions& Options,
		UAnimSequence*& OutSequence, FAnimationDataBuildReport& OutReport, FString& OutError);

	/**
	 * Writes a sequence as a new asset at PackageName (for example /Game/Animation/A_Wave) and saves
	 * it. InvalidInput is returned before any object exists. SaveFailed marks the in-memory sequence
	 * and package for garbage collection, but does not promise filesystem rollback: the engine's save
	 * path may leave partial files. A caller that needs a clean disk target must verify or remove it.
	 */
	static EAnimationDataResult WriteAsset(const FAnimationDataMappedClip& Clip, const FAnimationDataBuildOptions& Options,
		const FString& PackageName, UAnimSequence*& OutSequence, FAnimationDataBuildReport& OutReport, FString& OutError);
};
