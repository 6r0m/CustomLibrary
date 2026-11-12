#include "AnimationDataEditorLibrary.h"

EAnimationDataResult UAnimationDataEditorLibrary::WriteTransientSequence(const FAnimationDataMappedClip& Clip, const FAnimationDataBuildOptions& Options,
	UAnimSequence*& OutSequence, FAnimationDataBuildReport& OutReport, FString& OutError)
{
	return FAnimationDataSequenceWriter::WriteTransient(Clip, Options, OutSequence, OutReport, OutError);
}

EAnimationDataResult UAnimationDataEditorLibrary::WriteSequenceAsset(const FAnimationDataMappedClip& Clip, const FAnimationDataBuildOptions& Options,
	const FString& PackageName, UAnimSequence*& OutSequence, FAnimationDataBuildReport& OutReport, FString& OutError)
{
	return FAnimationDataSequenceWriter::WriteAsset(Clip, Options, PackageName, OutSequence, OutReport, OutError);
}
