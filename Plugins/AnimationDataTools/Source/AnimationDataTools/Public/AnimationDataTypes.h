#pragma once

#include "CoreMinimal.h"
#include "AnimationDataTypes.generated.h"

/** Result of a channel-animation operation. Nothing is created or changed unless the result is Ok. */
UENUM(BlueprintType)
enum class EAnimationDataResult : uint8
{
	Ok,
	/** The clip, profile or options are malformed; the accompanying message says why. */
	InvalidInput,
	/** The call came from a thread other than the game thread. */
	WrongThread,
	/** The asset could not be saved. */
	SaveFailed,
};

/** One named float channel, one value per frame. */
USTRUCT(BlueprintType)
struct ANIMATIONDATATOOLS_API FAnimationDataCurve
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	TArray<float> Values;
};

/** Per-frame local rotations of one bone, relative to its reference pose: the frame's rotation is Reference * Delta. */
USTRUCT(BlueprintType)
struct ANIMATIONDATATOOLS_API FAnimationDataBoneTrack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	FName Bone;

	/** Unit quaternions, one per frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	TArray<FQuat> Deltas;
};

/** Source data: channel-major curves and bone tracks sampled at FrameRate. Every curve and track has the same number of frames. */
USTRUCT(BlueprintType)
struct ANIMATIONDATATOOLS_API FAnimationDataClip
{
	GENERATED_BODY()

	/** Samples per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	float FrameRate = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	TArray<FAnimationDataCurve> Curves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	TArray<FAnimationDataBoneTrack> Bones;
};

/** Adds Source * Multiplier + Offset to Target, frame by frame. */
USTRUCT(BlueprintType)
struct ANIMATIONDATATOOLS_API FAnimationDataRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	FName Source;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	FName Target;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	float Multiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	float Offset = 0.0f;
};

/** Bounds a target curve after every contribution to it has been summed. */
USTRUCT(BlueprintType)
struct ANIMATIONDATATOOLS_API FAnimationDataLimit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	FName Target;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	float Min = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	float Max = 1.0f;
};

/** How a clip's channels become a skeleton's curves and bones. */
USTRUCT(BlueprintType)
struct ANIMATIONDATATOOLS_API FAnimationDataProfile
{
	GENERATED_BODY()

	/** Applied in order; contributions to one target are summed in this order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	TArray<FAnimationDataRule> Rules;

	/** Source bone name to target bone name. Bones not listed keep their names. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	TMap<FName, FName> BoneRenames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	TArray<FAnimationDataLimit> Limits;

	/** Source curves that no rule reads are copied to a target curve of the same name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	bool bPassThroughUnmapped = false;
};

/** Mapper output: target curves and bone tracks, every one with exactly NumFrames samples. */
USTRUCT(BlueprintType)
struct ANIMATIONDATATOOLS_API FAnimationDataMappedClip
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	float FrameRate = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	int32 NumFrames = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	TArray<FAnimationDataCurve> Curves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	TArray<FAnimationDataBoneTrack> Bones;

	/** Rule sources the clip did not contain. Their rules contributed nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Data Tools")
	TArray<FName> MissingSources;
};
