#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Async/Async.h"
#include "BlueprintCompileCheck.h"
#include "AnimationDataEditorLibrary.h"
#include "AnimationDataMapper.h"
#include "AnimationDataSequenceWriter.h"
#include "EditorUtilityObject.h"
#include "JsonObjectConverter.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PackageTools.h"
#include "UObject/StructOnScope.h"
#include "UObject/UObjectIterator.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, AnimationDataToolsEditorTests)

namespace
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr float Tolerance = 1e-4f;
	const TCHAR* SkeletonPath = TEXT("/Engine/EngineMeshes/SkeletalCube_Skeleton.SkeletalCube_Skeleton");
	const TCHAR* SavedPackage = TEXT("/Game/AnimationDataToolsTests/A_SaveReload");

	USkeleton* LoadSkeleton()
	{
		return LoadObject<USkeleton>(nullptr, SkeletonPath);
	}

	/** The skeleton's last bone, which is not its root. */
	FName ChildBone(const USkeleton* Skeleton)
	{
		const FReferenceSkeleton& Reference = Skeleton->GetReferenceSkeleton();
		return Reference.GetNum() > 1 ? Reference.GetBoneName(Reference.GetNum() - 1) : NAME_None;
	}

	FQuat RefRotation(const USkeleton* Skeleton, FName Bone)
	{
		const FReferenceSkeleton& Reference = Skeleton->GetReferenceSkeleton();
		return Reference.GetRefBonePose()[Reference.FindBoneIndex(Bone)].GetRotation();
	}

	FAnimationDataMappedClip MakeMapped(float FrameRate, TArray<FAnimationDataCurve> Curves, TArray<FAnimationDataBoneTrack> Bones = {})
	{
		FAnimationDataMappedClip Clip;
		Clip.FrameRate = FrameRate;
		Clip.NumFrames = !Curves.IsEmpty() ? Curves[0].Values.Num() : (!Bones.IsEmpty() ? Bones[0].Deltas.Num() : 0);
		Clip.Curves = MoveTemp(Curves);
		Clip.Bones = MoveTemp(Bones);
		return Clip;
	}

	FAnimationDataCurve MakeCurve(const TCHAR* Name, TArray<float> Values)
	{
		FAnimationDataCurve Curve;
		Curve.Name = Name;
		Curve.Values = MoveTemp(Values);
		return Curve;
	}

	FAnimationDataBoneTrack MakeTrack(FName Bone, TArray<FQuat> Deltas)
	{
		FAnimationDataBoneTrack Track;
		Track.Bone = Bone;
		Track.Deltas = MoveTemp(Deltas);
		return Track;
	}

	FAnimationDataBuildOptions Options(USkeleton* Skeleton)
	{
		FAnimationDataBuildOptions Result;
		Result.Skeleton = Skeleton;
		return Result;
	}

	UAnimSequence* WriteOk(FAutomationTestBase& Test, const FAnimationDataMappedClip& Clip, const FAnimationDataBuildOptions& BuildOptions, FAnimationDataBuildReport* OutReport = nullptr)
	{
		UAnimSequence* Sequence = nullptr;
		FAnimationDataBuildReport Report;
		FString Error;
		Test.TestEqual(FString::Printf(TEXT("write succeeds (%s)"), *Error), FAnimationDataSequenceWriter::WriteTransient(Clip, BuildOptions, Sequence, Report, Error), EAnimationDataResult::Ok);
		if (OutReport)
		{
			*OutReport = Report;
		}
		return Sequence;
	}

	float EvalCurve(const UAnimSequence* Sequence, const TCHAR* Name, double Time)
	{
		const FFloatCurve* Curve = Sequence->GetDataModel()->FindFloatCurve(FAnimationCurveIdentifier(Name, ERawCurveTrackTypes::RCT_Float));
		return Curve ? Curve->Evaluate(static_cast<float>(Time)) : NAN;
	}

	FQuat EvalRotation(const UAnimSequence* Sequence, FName Bone, double Time)
	{
		const IAnimationDataModel* Model = Sequence->GetDataModel();
		return Model->EvaluateBoneTrackTransform(Bone, Model->GetFrameRate().AsFrameTime(Time), Sequence->Interpolation).GetRotation();
	}

	int32 CountSequences()
	{
		int32 Count = 0;
		for (TObjectIterator<UAnimSequence> It; It; ++It)
		{
			Count += IsValid(*It);
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWriterNeutral, "AnimationDataToolsEditor.Writer.N2ZeroKeys", TestFlags)
bool FWriterNeutral::RunTest(const FString& Parameters)
{
	USkeleton* Skeleton = LoadSkeleton();
	UAnimSequence* Sequence = WriteOk(*this, MakeMapped(30.0f, {MakeCurve(TEXT("Smile"), {0.0f, 1.0f, 0.0f})}), Options(Skeleton));
	if (!TestNotNull(TEXT("a sequence"), Sequence))
	{
		return false;
	}
	const FFloatCurve* Curve = Sequence->GetDataModel()->FindFloatCurve(FAnimationCurveIdentifier(TEXT("Smile"), ERawCurveTrackTypes::RCT_Float));
	TestEqual(TEXT("N + 1 keys, zero values included"), Curve ? Curve->FloatCurve.GetNumKeys() : 0, 4);
	TestEqual(TEXT("zero at the start"), EvalCurve(Sequence, TEXT("Smile"), 0.0), 0.0f, Tolerance);
	TestEqual(TEXT("one in the middle"), EvalCurve(Sequence, TEXT("Smile"), 1.0 / 30.0), 1.0f, Tolerance);
	TestEqual(TEXT("zero at the end"), EvalCurve(Sequence, TEXT("Smile"), 3.0 / 30.0), 0.0f, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWriterRate, "AnimationDataToolsEditor.Writer.N6Rate", TestFlags)
bool FWriterRate::RunTest(const FString& Parameters)
{
	TArray<float> Values;
	Values.SetNumZeroed(31);
	UAnimSequence* Sequence = WriteOk(*this, MakeMapped(30.0f, {MakeCurve(TEXT("C"), Values)}), Options(LoadSkeleton()));
	if (!TestNotNull(TEXT("a sequence"), Sequence))
	{
		return false;
	}
	TestEqual(TEXT("length is samples / rate"), static_cast<double>(Sequence->GetPlayLength()), 31.0 / 30.0, 1e-5);
	TestEqual(TEXT("frames"), Sequence->GetDataModel()->GetNumberOfFrames(), 31);
	TestEqual(TEXT("keys"), Sequence->GetDataModel()->GetNumberOfKeys(), 32);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWriterBone, "AnimationDataToolsEditor.Writer.N8Bone", TestFlags)
bool FWriterBone::RunTest(const FString& Parameters)
{
	USkeleton* Skeleton = LoadSkeleton();
	const FName Bone = ChildBone(Skeleton);
	if (!TestFalse(TEXT("the engine skeleton has a child bone"), Bone.IsNone()))
	{
		return false;
	}
	FAnimationDataClip Source;
	Source.Bones = {MakeTrack(TEXT("Head"), {FQuat(FVector::UpVector, UE_HALF_PI)})};
	FAnimationDataProfile Profile;
	Profile.BoneRenames.Add(TEXT("Head"), Bone);
	FAnimationDataMappedClip Mapped;
	FString Error;
	FAnimationDataMapper::Map(Source, Profile, Mapped, Error);
	UAnimSequence* Sequence = WriteOk(*this, Mapped, Options(Skeleton));
	if (!TestNotNull(TEXT("a sequence"), Sequence))
	{
		return false;
	}
	const FQuat Expected = RefRotation(Skeleton, Bone) * FQuat(FVector::UpVector, UE_HALF_PI);
	TestTrue(TEXT("the renamed bone is Reference * Delta"), EvalRotation(Sequence, Bone, 0.0).Equals(Expected, Tolerance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWriterMetadata, "AnimationDataToolsEditor.Writer.N10CurveMetadata", TestFlags)
bool FWriterMetadata::RunTest(const FString& Parameters)
{
	USkeleton* Skeleton = LoadSkeleton();
	const FAnimationDataMappedClip Mapped = MakeMapped(30.0f, {MakeCurve(TEXT("NotOnSkeleton"), {0.5f, 0.25f})});
	FAnimationDataBuildReport Report;
	UAnimSequence* Kept = WriteOk(*this, Mapped, Options(Skeleton), &Report);
	TestTrue(TEXT("the missing curve is listed"), Report.CurvesMissingFromSkeleton.Contains(FName(TEXT("NotOnSkeleton"))));
	TestFalse(TEXT("and kept by default"), Report.bMissingCurvesDropped);
	TestEqual(TEXT("kept curve value"), Kept ? EvalCurve(Kept, TEXT("NotOnSkeleton"), 0.0) : NAN, 0.5f, Tolerance);
	FAnimationDataBuildOptions Drop = Options(Skeleton);
	Drop.bDropCurvesMissingFromSkeleton = true;
	UAnimSequence* Dropped = WriteOk(*this, Mapped, Drop, &Report);
	TestTrue(TEXT("dropping is reported"), Report.bMissingCurvesDropped);
	TestTrue(TEXT("and the curve is absent"), Dropped && !Dropped->GetDataModel()->FindFloatCurve(FAnimationCurveIdentifier(TEXT("NotOnSkeleton"), ERawCurveTrackTypes::RCT_Float)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWriterOneSample, "AnimationDataToolsEditor.Writer.N13OneSample", TestFlags)
bool FWriterOneSample::RunTest(const FString& Parameters)
{
	USkeleton* Skeleton = LoadSkeleton();
	const FName Bone = ChildBone(Skeleton);
	const FQuat Delta(FVector::ForwardVector, UE_PI / 4.0);
	UAnimSequence* Sequence = WriteOk(*this, MakeMapped(30.0f, {MakeCurve(TEXT("C"), {0.7f})}, {MakeTrack(Bone, {Delta})}), Options(Skeleton));
	if (!TestNotNull(TEXT("a sequence"), Sequence))
	{
		return false;
	}
	const FQuat Expected = RefRotation(Skeleton, Bone) * Delta;
	for (const double Time : {0.0, 0.5 / 30.0, 1.0 / 30.0})
	{
		TestEqual(FString::Printf(TEXT("curve held at %.4f s"), Time), EvalCurve(Sequence, TEXT("C"), Time), 0.7f, Tolerance);
		TestTrue(FString::Printf(TEXT("bone held at %.4f s"), Time), EvalRotation(Sequence, Bone, Time).Equals(Expected, Tolerance));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWriterTwoSamples, "AnimationDataToolsEditor.Writer.N14TwoSamples", TestFlags)
bool FWriterTwoSamples::RunTest(const FString& Parameters)
{
	USkeleton* Skeleton = LoadSkeleton();
	const FName Bone = ChildBone(Skeleton);
	const FQuat Turned(FVector::UpVector, UE_HALF_PI);
	UAnimSequence* Sequence = WriteOk(*this, MakeMapped(30.0f, {MakeCurve(TEXT("C"), {0.0f, 1.0f})}, {MakeTrack(Bone, {FQuat::Identity, Turned})}), Options(Skeleton));
	if (!TestNotNull(TEXT("a sequence"), Sequence))
	{
		return false;
	}
	const FQuat Ref = RefRotation(Skeleton, Bone);
	TestEqual(TEXT("curve interpolates at 0.5 / F"), EvalCurve(Sequence, TEXT("C"), 0.5 / 30.0), 0.5f, Tolerance);
	TestTrue(TEXT("bone interpolates at 0.5 / F"), EvalRotation(Sequence, Bone, 0.5 / 30.0).Equals(Ref * FQuat(FVector::UpVector, UE_HALF_PI / 2.0), Tolerance));
	for (const double Time : {1.0 / 30.0, 1.5 / 30.0, 2.0 / 30.0})
	{
		TestEqual(FString::Printf(TEXT("curve held at %.4f s"), Time), EvalCurve(Sequence, TEXT("C"), Time), 1.0f, Tolerance);
		TestTrue(FString::Printf(TEXT("bone held at %.4f s"), Time), EvalRotation(Sequence, Bone, Time).Equals(Ref * Turned, Tolerance));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWriterRefusal, "AnimationDataToolsEditor.Writer.N7Refusal", TestFlags)
bool FWriterRefusal::RunTest(const FString& Parameters)
{
	USkeleton* Skeleton = LoadSkeleton();
	FAnimationDataMappedClip Short = MakeMapped(30.0f, {MakeCurve(TEXT("C"), {0.0f, 0.0f, 0.0f})}, {MakeTrack(ChildBone(Skeleton), {FQuat::Identity, FQuat::Identity})});
	Short.NumFrames = 3;
	const TArray<TTuple<const TCHAR*, FAnimationDataMappedClip, USkeleton*>> Cases = {
		{TEXT("a short bone track"), Short, Skeleton},
		{TEXT("a bone the skeleton lacks"), MakeMapped(30.0f, {}, {MakeTrack(TEXT("NoSuchBone"), {FQuat::Identity})}), Skeleton},
		{TEXT("no frames"), MakeMapped(30.0f, {MakeCurve(TEXT("C"), {})}), Skeleton},
		{TEXT("no skeleton"), MakeMapped(30.0f, {MakeCurve(TEXT("C"), {0.0f})}), nullptr},
		{TEXT("a frame rate below the representable minimum"), MakeMapped(0.0004f, {MakeCurve(TEXT("C"), {0.0f})}), Skeleton},
		{TEXT("a frame rate beyond the representable maximum"), MakeMapped(1.0e10f, {MakeCurve(TEXT("C"), {0.0f})}), Skeleton},
		{TEXT("a rate the animation data controller rejects"), MakeMapped(1.0e9f, {MakeCurve(TEXT("C"), {0.0f})}), Skeleton},
	};
	const int32 Before = CountSequences();
	for (const auto& [Name, Clip, CaseSkeleton] : Cases)
	{
		UAnimSequence* Sequence = nullptr;
		FAnimationDataBuildReport Report;
		FString Error;
		TestEqual(FString::Printf(TEXT("%s is refused"), Name), FAnimationDataSequenceWriter::WriteTransient(Clip, Options(CaseSkeleton), Sequence, Report, Error), EAnimationDataResult::InvalidInput);
		TestNull(FString::Printf(TEXT("%s returns no sequence"), Name), Sequence);
		TestFalse(FString::Printf(TEXT("%s has a reason"), Name), Error.IsEmpty());
	}
	TestEqual(TEXT("nothing was created"), CountSequences(), Before);
	UAnimSequence* Tiny = WriteOk(*this, MakeMapped(0.001f, {MakeCurve(TEXT("C"), {0.5f})}), Options(Skeleton));
	TestTrue(TEXT("the minimum representable rate has finite timing"),
		Tiny && FMath::IsFinite(Tiny->GetPlayLength()) && FMath::IsFinite(Tiny->GetDataModel()->GetFrameRate().AsSeconds(FFrameNumber(1))));
	UAnimSequence* Sequence = nullptr;
	FAnimationDataBuildReport Report;
	FString Error;
	TestEqual(TEXT("an invalid package name is refused"),
		FAnimationDataSequenceWriter::WriteAsset(MakeMapped(30.0f, {MakeCurve(TEXT("C"), {0.0f})}), Options(Skeleton), TEXT("not a package"), Sequence, Report, Error), EAnimationDataResult::InvalidInput);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWriterSaveReload, "AnimationDataToolsEditor.Writer.G7SaveReload", TestFlags)
bool FWriterSaveReload::RunTest(const FString& Parameters)
{
	USkeleton* Skeleton = LoadSkeleton();
	const FName Bone = ChildBone(Skeleton);
	const FQuat Delta(FVector::UpVector, UE_HALF_PI);
	const FAnimationDataMappedClip Clip = MakeMapped(24.0f, {MakeCurve(TEXT("C"), {0.0f, 0.4f, 0.8f})}, {MakeTrack(Bone, {FQuat::Identity, Delta, Delta})});
	UAnimSequence* Sequence = nullptr;
	FAnimationDataBuildReport Report;
	FString Error;
	TestEqual(FString::Printf(TEXT("write the asset (%s)"), *Error), FAnimationDataSequenceWriter::WriteAsset(Clip, Options(Skeleton), SavedPackage, Sequence, Report, Error), EAnimationDataResult::Ok);
	if (!TestNotNull(TEXT("an asset"), Sequence))
	{
		return false;
	}
	const FString ObjectPath = Sequence->GetPathName();
	TestTrue(TEXT("the package is on disk"), FPaths::FileExists(FPackageName::LongPackageNameToFilename(SavedPackage, FPackageName::GetAssetPackageExtension())));
	const TArray<UPackage*> Packages = {Sequence->GetPackage()};
	TestTrue(TEXT("unload"), UPackageTools::UnloadPackages(Packages));
	UAnimSequence* Reloaded = LoadObject<UAnimSequence>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("reload"), Reloaded))
	{
		return false;
	}
	TestEqual(TEXT("reloaded length"), static_cast<double>(Reloaded->GetPlayLength()), 3.0 / 24.0, 1e-5);
	TestEqual(TEXT("reloaded curve"), EvalCurve(Reloaded, TEXT("C"), 1.0 / 24.0), 0.4f, Tolerance);
	TestEqual(TEXT("reloaded final hold"), EvalCurve(Reloaded, TEXT("C"), 3.0 / 24.0), 0.8f, Tolerance);
	TestTrue(TEXT("reloaded bone"), EvalRotation(Reloaded, Bone, 2.0 / 24.0).Equals(RefRotation(Skeleton, Bone) * Delta, Tolerance));
	UAnimSequence* Second = nullptr;
	TestEqual(TEXT("an existing asset is never overwritten"), FAnimationDataSequenceWriter::WriteAsset(Clip, Options(Skeleton), SavedPackage, Second, Report, Error), EAnimationDataResult::InvalidInput);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWriterThread, "AnimationDataToolsEditor.Writer.WrongThread", TestFlags)
bool FWriterThread::RunTest(const FString& Parameters)
{
	USkeleton* Skeleton = LoadSkeleton();
	const FAnimationDataMappedClip Clip = MakeMapped(30.0f, {MakeCurve(TEXT("C"), {0.0f})});
	const EAnimationDataResult Result = Async(EAsyncExecution::Thread, [&]()
	{
		UAnimSequence* Sequence = nullptr;
		FAnimationDataBuildReport Report;
		FString Error;
		return FAnimationDataSequenceWriter::WriteTransient(Clip, Options(Skeleton), Sequence, Report, Error);
	}).Get();
	TestEqual(TEXT("writing off the game thread is refused"), Result, EAnimationDataResult::WrongThread);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWriterReflectedCall, "AnimationDataToolsEditor.Blueprint.ReflectedCall", TestFlags)
bool FWriterReflectedCall::RunTest(const FString& Parameters)
{
	UFunction* Function = UAnimationDataEditorLibrary::StaticClass()->FindFunctionByName(TEXT("WriteTransientSequence"));
	if (!TestNotNull(TEXT("WriteTransientSequence is reflected"), Function))
	{
		return false;
	}
	FStructOnScope Params(Function);
	*CastField<FStructProperty>(Function->FindPropertyByName(TEXT("Clip")))->ContainerPtrToValuePtr<FAnimationDataMappedClip>(Params.GetStructMemory()) = MakeMapped(30.0f, {MakeCurve(TEXT("C"), {0.25f})});
	*CastField<FStructProperty>(Function->FindPropertyByName(TEXT("Options")))->ContainerPtrToValuePtr<FAnimationDataBuildOptions>(Params.GetStructMemory()) = Options(LoadSkeleton());
	UAnimationDataEditorLibrary::StaticClass()->GetDefaultObject()->ProcessEvent(Function, Params.GetStructMemory());
	UAnimSequence* Sequence = Cast<UAnimSequence>(CastField<FObjectProperty>(Function->FindPropertyByName(TEXT("OutSequence")))->GetObjectPropertyValue_InContainer(Params.GetStructMemory()));
	TestEqual(TEXT("reflected result"), *CastField<FEnumProperty>(Function->GetReturnProperty())->ContainerPtrToValuePtr<uint8>(Params.GetStructMemory()), static_cast<uint8>(EAnimationDataResult::Ok));
	TestTrue(TEXT("reflected call writes the sequence"), Sequence && FMath::IsNearlyEqual(EvalCurve(Sequence, TEXT("C"), 0.0), 0.25f, Tolerance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWriterBlueprintCompile, "AnimationDataToolsEditor.Blueprint.GraphCompiles", TestFlags)
bool FWriterBlueprintCompile::RunTest(const FString& Parameters)
{
	const TArray<UFunction*> Functions = HostTestSupport::BlueprintCallableFunctions(UAnimationDataEditorLibrary::StaticClass());
	TestEqual(TEXT("both writers are callable"), Functions.Num(), 2);
	// Editor-module nodes belong in editor utility Blueprints; a runtime Blueprint must refuse them.
	FString Messages;
	TestTrue(FString::Printf(TEXT("an editor utility graph calling every function compiles:\n%s"), *Messages),
		HostTestSupport::CompileCallsInBlueprint(Functions, Messages, UEditorUtilityObject::StaticClass()));
	// One compiler error per editor function is expected from the runtime Blueprint.
	AddExpectedErrorPlain(TEXT("Only for use in Editor Utility Blueprints"), EAutomationExpectedErrorFlags::Contains, 2);
	FString RuntimeMessages;
	TestFalse(TEXT("a runtime Blueprint refuses the editor nodes"), HostTestSupport::CompileCallsInBlueprint(Functions, RuntimeMessages));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWriterExample, "AnimationDataToolsEditor.Example.JsonClipToSequence", TestFlags)
bool FWriterExample::RunTest(const FString& Parameters)
{
	// Example: a clip arrives as JSON (from a file or a service), is mapped with a profile and
	// written as a sequence for the editor's animation tools.
	const FString Json = TEXT(R"({ "FrameRate": 25, "Curves": [ { "Name": "Blink", "Values": [0, 100, 0] } ], "Bones": [] })");
	FAnimationDataClip Clip;
	if (!TestTrue(TEXT("the JSON is a clip"), FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Clip)))
	{
		return false;
	}
	FAnimationDataProfile Profile;
	FAnimationDataRule& Rule = Profile.Rules.AddDefaulted_GetRef();
	Rule.Source = TEXT("Blink");
	Rule.Target = TEXT("EyeClosed");
	Rule.Multiplier = 0.01f;
	FAnimationDataMappedClip Mapped;
	FString Error;
	TestEqual(TEXT("map"), FAnimationDataMapper::Map(Clip, Profile, Mapped, Error), EAnimationDataResult::Ok);
	UAnimSequence* Sequence = WriteOk(*this, Mapped, Options(LoadSkeleton()));
	TestTrue(TEXT("the blink peaks at the middle frame"), Sequence && FMath::IsNearlyEqual(EvalCurve(Sequence, TEXT("EyeClosed"), 1.0 / 25.0), 1.0f, Tolerance));
	return true;
}
