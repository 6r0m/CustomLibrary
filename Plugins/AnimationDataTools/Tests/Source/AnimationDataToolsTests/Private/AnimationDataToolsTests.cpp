#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimSequence.h"
#include "Animation/PoseAsset.h"
#include "Animation/Skeleton.h"
#include "Async/Async.h"
#include "BlueprintCompileCheck.h"
#include "Cases/MappingCases.h"
#include "AnimationDataBlueprintLibrary.h"
#include "AnimationDataMapper.h"
#include "AnimationDataProfileAsset.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/StructOnScope.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, AnimationDataToolsTests)

namespace
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	using namespace MappingCases;

	FAnimationDataMappedClip MapOk(FAutomationTestBase& Test, const FAnimationDataClip& Clip, const FAnimationDataProfile& Profile)
	{
		FAnimationDataMappedClip Mapped;
		FString Error;
		Test.TestEqual(FString::Printf(TEXT("map succeeds (%s)"), *Error), FAnimationDataMapper::Map(Clip, Profile, Mapped, Error), EAnimationDataResult::Ok);
		return Mapped;
	}

	bool ValuesNear(const FAnimationDataCurve* Curve, const TArray<float>& Expected)
	{
		if (!Curve || Curve->Values.Num() != Expected.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Expected.Num(); ++Index)
		{
			if (!FMath::IsNearlyEqual(Curve->Values[Index], Expected[Index], Tolerance))
			{
				return false;
			}
		}
		return true;
	}

	/** A pose asset whose poses A, B and C hold Target = 0.1, 0.2 and 0.3, built the way the editor builds one: from a sequence's keys. */
	UPoseAsset* MakePoseAsset(USkeleton* Skeleton)
	{
		UAnimSequence* Source = NewObject<UAnimSequence>(GetTransientPackage(), NAME_None, RF_Transient);
		Source->SetSkeleton(Skeleton);
		IAnimationDataController& Controller = Source->GetController();
		Controller.InitializeModel();
		Controller.OpenBracket(FText::FromString(TEXT("Pose source")), false);
		Controller.SetFrameRate(FFrameRate(30, 1), false);
		Controller.SetNumberOfFrames(FFrameNumber(2), false);
		const FReferenceSkeleton& Reference = Skeleton->GetReferenceSkeleton();
		const FName Root = Reference.GetBoneName(0);
		const FTransform& RootPose = Reference.GetRefBonePose()[0];
		Controller.AddBoneCurve(Root, false);
		Controller.SetBoneTrackKeys(Root, TArray<FVector>{RootPose.GetTranslation(), RootPose.GetTranslation(), RootPose.GetTranslation()},
			TArray<FQuat>{RootPose.GetRotation(), RootPose.GetRotation(), RootPose.GetRotation()}, TArray<FVector>{FVector::OneVector, FVector::OneVector, FVector::OneVector}, false);
		const FAnimationCurveIdentifier Id(TEXT("Target"), ERawCurveTrackTypes::RCT_Float);
		Controller.AddCurve(Id, AACF_DefaultCurve, false);
		TArray<FRichCurveKey> Keys;
		for (int32 Key = 0; Key < 3; ++Key)
		{
			Keys.Emplace(Key / 30.0f, 0.1f * (Key + 1));
		}
		Controller.SetCurveKeys(Id, Keys, false);
		Controller.NotifyPopulated();
		Controller.CloseBracket(false);

		UPoseAsset* Pose = NewObject<UPoseAsset>(GetTransientPackage(), NAME_None, RF_Transient);
		const TArray<FName> PoseNames = {TEXT("A"), TEXT("B"), TEXT("C")};
		Pose->CreatePoseFromAnimation(Source, &PoseNames);
		return Pose;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapperRenamed, "AnimationDataTools.Mapper.N1Renamed", TestFlags)
bool FMapperRenamed::RunTest(const FString& Parameters)
{
	FAnimationDataProfile Profile;
	Profile.Rules = {MakeRule(TEXT("Blink"), TEXT("LidL"), 0.01f)};
	const FAnimationDataMappedClip Mapped = MapOk(*this, MakeClip({MakeCurve(TEXT("Blink"), {0.0f, 50.0f, 100.0f})}), Profile);
	TestTrue(TEXT("the renamed target is keyed"), ValuesNear(Find(Mapped, TEXT("LidL")), {0.0f, 0.5f, 1.0f}));
	TestNull(TEXT("the source name is not a target"), Find(Mapped, TEXT("Blink")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapperNeutral, "AnimationDataTools.Mapper.N2Neutral", TestFlags)
bool FMapperNeutral::RunTest(const FString& Parameters)
{
	FAnimationDataProfile Profile;
	Profile.Rules = {MakeRule(TEXT("Smile"), TEXT("MouthSmile"))};
	const FAnimationDataMappedClip Mapped = MapOk(*this, MakeClip({MakeCurve(TEXT("Smile"), {0.0f, 1.0f, 0.0f})}), Profile);
	TestTrue(TEXT("zero frames are keyed too"), ValuesNear(Find(Mapped, TEXT("MouthSmile")), {0.0f, 1.0f, 0.0f}));
	TestEqual(TEXT("frame count"), Mapped.NumFrames, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapperAccumulate, "AnimationDataTools.Mapper.N3Accumulate", TestFlags)
bool FMapperAccumulate::RunTest(const FString& Parameters)
{
	FAnimationDataProfile Profile;
	Profile.Rules = {MakeRule(TEXT("A"), TEXT("Target"), 0.1f), MakeRule(TEXT("B"), TEXT("Target"), 0.2f), MakeRule(TEXT("C"), TEXT("Target"), 0.3f)};
	const FAnimationDataClip Ones = MakeClip({MakeCurve(TEXT("A"), {1.0f, 1.0f}), MakeCurve(TEXT("B"), {1.0f, 1.0f}), MakeCurve(TEXT("C"), {1.0f, 1.0f})});
	TestTrue(TEXT("a profile sums its contributions"), ValuesNear(Find(MapOk(*this, Ones, Profile), TEXT("Target")), {0.6f, 0.6f}));

	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, SkeletonPath());
	if (!TestNotNull(TEXT("the engine skeleton loads"), Skeleton))
	{
		return false;
	}
	FAnimationDataProfile FromPoses;
	FString Error;
	TestEqual(TEXT("pose asset converts"), FAnimationDataMapper::MakeRulesFromPoseAsset(MakePoseAsset(Skeleton), FromPoses.Rules, Error), EAnimationDataResult::Ok);
	TestEqual(TEXT("one rule per pose"), FromPoses.Rules.Num(), 3);
	TestTrue(TEXT("a pose asset sums the same way"), ValuesNear(Find(MapOk(*this, Ones, FromPoses), TEXT("Target")), {0.6f, 0.6f}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapperWide, "AnimationDataTools.Mapper.N4Wide", TestFlags)
bool FMapperWide::RunTest(const FString& Parameters)
{
	FAnimationDataClip Source = MakeClip({});
	FAnimationDataProfile Profile;
	for (int32 Channel = 0; Channel < WideChannels; ++Channel)
	{
		Source.Curves.Add(MakeCurve(*FString::Printf(TEXT("In%d"), Channel), {static_cast<float>(Channel), 0.0f}));
		Profile.Rules.Add(MakeRule(*FString::Printf(TEXT("In%d"), Channel), *FString::Printf(TEXT("Out%d"), Channel)));
	}
	const FAnimationDataMappedClip Mapped = MapOk(*this, Source, Profile);
	TestEqual(TEXT("every channel maps"), Mapped.Curves.Num(), WideChannels);
	TestTrue(TEXT("the last channel is keyed"), ValuesNear(Find(Mapped, *FString::Printf(TEXT("Out%d"), WideChannels - 1)), {static_cast<float>(WideChannels - 1), 0.0f}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapperRange, "AnimationDataTools.Mapper.N5Range", TestFlags)
bool FMapperRange::RunTest(const FString& Parameters)
{
	FAnimationDataProfile Profile;
	Profile.Rules = {MakeRule(TEXT("X"), TEXT("T")), MakeRule(TEXT("Y"), TEXT("T"))};
	const FAnimationDataClip Source = MakeClip({MakeCurve(TEXT("X"), {-0.5f, 1.5f, 1.5f}), MakeCurve(TEXT("Y"), {0.0f, 0.0f, -0.9f})});
	TestTrue(TEXT("no implicit clamp"), ValuesNear(Find(MapOk(*this, Source, Profile), TEXT("T")), {-0.5f, 1.5f, 0.6f}));
	FAnimationDataLimit Limit;
	Limit.Target = TEXT("T");
	Profile.Limits = {Limit};
	// 1.5 + -0.9 stays 0.6: the limit applies to the sum, not to each contribution.
	TestTrue(TEXT("limits apply after the sum"), ValuesNear(Find(MapOk(*this, Source, Profile), TEXT("T")), {0.0f, 1.0f, 0.6f}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapperShortBone, "AnimationDataTools.Mapper.N7ShortBone", TestFlags)
bool FMapperShortBone::RunTest(const FString& Parameters)
{
	FAnimationDataClip Source = MakeClip({MakeCurve(TEXT("A"), {0.0f, 0.0f, 0.0f})});
	FAnimationDataBoneTrack Track;
	Track.Bone = TEXT("Head");
	Track.Deltas = {FQuat::Identity, FQuat::Identity};
	Source.Bones = {Track};
	FAnimationDataMappedClip Mapped;
	FString Error;
	TestEqual(TEXT("a short bone track is refused"), FAnimationDataMapper::Map(Source, FAnimationDataProfile(), Mapped, Error), EAnimationDataResult::InvalidInput);
	TestTrue(TEXT("with a reason"), Error.Contains(TEXT("Head")));
	TestEqual(TEXT("nothing is produced"), Mapped.NumFrames, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapperBone, "AnimationDataTools.Mapper.N8BoneRename", TestFlags)
bool FMapperBone::RunTest(const FString& Parameters)
{
	FAnimationDataClip Source = MakeClip({});
	FAnimationDataBoneTrack Track;
	Track.Bone = TEXT("Head");
	Track.Deltas = {FQuat(FVector::UpVector, UE_HALF_PI)};
	Source.Bones = {Track};
	FAnimationDataProfile Profile;
	Profile.BoneRenames.Add(TEXT("Head"), TEXT("Bone02"));
	const FAnimationDataMappedClip Mapped = MapOk(*this, Source, Profile);
	TestEqual(TEXT("one bone"), Mapped.Bones.Num(), 1);
	TestEqual(TEXT("renamed"), Mapped.Bones[0].Bone, FName(TEXT("Bone02")));
	TestTrue(TEXT("deltas kept"), Mapped.Bones[0].Deltas[0].Equals(Track.Deltas[0]));
	Profile.BoneRenames.Add(TEXT("Neck"), TEXT("Bone02"));
	FAnimationDataBoneTrack Neck = Track;
	Neck.Bone = TEXT("Neck");
	Source.Bones.Add(Neck);
	FAnimationDataMappedClip Collided;
	FString Error;
	TestEqual(TEXT("two bones renamed to one are refused"), FAnimationDataMapper::Map(Source, Profile, Collided, Error), EAnimationDataResult::InvalidInput);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapperMalformed, "AnimationDataTools.Mapper.N9Malformed", TestFlags)
bool FMapperMalformed::RunTest(const FString& Parameters)
{
	FAnimationDataProfile Valid;
	FAnimationDataProfile BadLimit;
	FAnimationDataLimit Limit;
	Limit.Target = TEXT("T");
	Limit.Min = 1.0f;
	Limit.Max = 0.0f;
	BadLimit.Limits = {Limit};
	FAnimationDataProfile Unnamed;
	Unnamed.Rules = {MakeRule(TEXT("A"), TEXT(""))};
	FAnimationDataClip NotUnit = MakeClip({});
	FAnimationDataBoneTrack Track;
	Track.Bone = TEXT("Head");
	Track.Deltas = {FQuat(0.0, 0.0, 0.0, 2.0)};
	NotUnit.Bones = {Track};

	const TArray<TTuple<const TCHAR*, FAnimationDataClip, FAnimationDataProfile>> Cases = {
		{TEXT("zero frames"), MakeClip({MakeCurve(TEXT("A"), {})}), Valid},
		{TEXT("no channels"), MakeClip({}), Valid},
		{TEXT("length mismatch"), MakeClip({MakeCurve(TEXT("A"), {0.0f, 1.0f}), MakeCurve(TEXT("B"), {0.0f})}), Valid},
		{TEXT("NaN"), MakeClip({MakeCurve(TEXT("A"), {0.0f, NAN})}), Valid},
		{TEXT("duplicate names"), MakeClip({MakeCurve(TEXT("A"), {0.0f}), MakeCurve(TEXT("A"), {1.0f})}), Valid},
		{TEXT("zero frame rate"), MakeClip({MakeCurve(TEXT("A"), {0.0f})}, 0.0f), Valid},
		{TEXT("not a unit rotation"), NotUnit, Valid},
		{TEXT("limit Min above Max"), MakeClip({MakeCurve(TEXT("A"), {0.0f})}), BadLimit},
		{TEXT("unnamed rule target"), MakeClip({MakeCurve(TEXT("A"), {0.0f})}), Unnamed},
	};
	for (const auto& [Name, Source, Profile] : Cases)
	{
		FAnimationDataMappedClip Mapped;
		FString Error;
		TestEqual(FString::Printf(TEXT("%s is refused"), Name), FAnimationDataMapper::Map(Source, Profile, Mapped, Error), EAnimationDataResult::InvalidInput);
		TestFalse(FString::Printf(TEXT("%s has a reason"), Name), Error.IsEmpty());
		TestTrue(FString::Printf(TEXT("%s produces nothing"), Name), Mapped.Curves.IsEmpty() && Mapped.Bones.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapperOverflow, "AnimationDataTools.Mapper.N15Overflow", TestFlags)
bool FMapperOverflow::RunTest(const FString& Parameters)
{
	{
		FAnimationDataProfile Profile;
		Profile.Rules = {MakeRule(TEXT("Big"), TEXT("Target"), 1.0e30f)};
		FAnimationDataMappedClip Mapped;
		FString Error;
		TestEqual(TEXT("multiplication overflow is refused"),
			FAnimationDataMapper::Map(MakeClip({MakeCurve(TEXT("Big"), {1.0e30f})}), Profile, Mapped, Error), EAnimationDataResult::InvalidInput);
		TestTrue(TEXT("the multiplication reason names the target and frame"), Error.Contains(TEXT("Target")) && Error.Contains(TEXT("frame 0")));
		TestTrue(TEXT("nothing is published"), Mapped.Curves.IsEmpty() && Mapped.Bones.IsEmpty());
	}
	{
		FAnimationDataProfile Profile;
		Profile.Rules = {MakeRule(TEXT("Max"), TEXT("Target")), MakeRule(TEXT("Max"), TEXT("Target"))};
		FAnimationDataMappedClip Mapped;
		FString Error;
		TestEqual(TEXT("accumulation overflow is refused"),
			FAnimationDataMapper::Map(MakeClip({MakeCurve(TEXT("Max"), {3.0e38f})}), Profile, Mapped, Error), EAnimationDataResult::InvalidInput);
		TestTrue(TEXT("the accumulation reason names the target and frame"), Error.Contains(TEXT("Target")) && Error.Contains(TEXT("frame 0")));
		TestTrue(TEXT("nothing is published after accumulation overflow"), Mapped.Curves.IsEmpty() && Mapped.Bones.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapperOrder, "AnimationDataTools.Mapper.N11Order", TestFlags)
bool FMapperOrder::RunTest(const FString& Parameters)
{
	FAnimationDataClip Source = MakeClip({});
	FAnimationDataProfile Profile;
	for (int32 Channel = 0; Channel < 12; ++Channel)
	{
		Source.Curves.Add(MakeCurve(*FString::Printf(TEXT("In%d"), Channel), {0.1f * Channel, 1.0f / (Channel + 3), 3.3f}));
		Profile.Rules.Add(MakeRule(*FString::Printf(TEXT("In%d"), Channel), Channel % 2 ? TEXT("Odd") : TEXT("Even"), 0.37f * Channel, 0.01f));
	}
	FRandomStream Stream(PermutationSeed);
	for (int32 Permutation = 0; Permutation < 5; ++Permutation)
	{
		const FAnimationDataMappedClip First = MapOk(*this, Source, Profile);
		const FAnimationDataMappedClip Second = MapOk(*this, Source, Profile);
		bool bIdentical = First.Curves.Num() == Second.Curves.Num();
		for (int32 Index = 0; bIdentical && Index < First.Curves.Num(); ++Index)
		{
			bIdentical = First.Curves[Index].Name == Second.Curves[Index].Name
				&& FMemory::Memcmp(First.Curves[Index].Values.GetData(), Second.Curves[Index].Values.GetData(), First.Curves[Index].Values.Num() * sizeof(float)) == 0;
		}
		TestTrue(FString::Printf(TEXT("permutation %d maps identically every time"), Permutation), bIdentical);
		for (int32 Index = Profile.Rules.Num() - 1; Index > 0; --Index)
		{
			Profile.Rules.Swap(Index, Stream.RandRange(0, Index));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapperOneToMany, "AnimationDataTools.Mapper.N12OneToMany", TestFlags)
bool FMapperOneToMany::RunTest(const FString& Parameters)
{
	FAnimationDataProfile Profile;
	Profile.Rules = {MakeRule(TEXT("Jaw"), TEXT("JawOpen"), 2.0f), MakeRule(TEXT("Jaw"), TEXT("MouthStretch"), 0.5f, 0.1f)};
	const FAnimationDataMappedClip Mapped = MapOk(*this, MakeClip({MakeCurve(TEXT("Jaw"), {0.0f, 0.4f})}), Profile);
	TestTrue(TEXT("first target"), ValuesNear(Find(Mapped, TEXT("JawOpen")), {0.0f, 0.8f}));
	TestTrue(TEXT("second target, with its offset"), ValuesNear(Find(Mapped, TEXT("MouthStretch")), {0.1f, 0.3f}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapperPassThrough, "AnimationDataTools.Mapper.PassThroughAndMissingSources", TestFlags)
bool FMapperPassThrough::RunTest(const FString& Parameters)
{
	FAnimationDataProfile Profile;
	Profile.Rules = {MakeRule(TEXT("A"), TEXT("Mapped")), MakeRule(TEXT("Absent"), TEXT("Mapped"))};
	const FAnimationDataClip Source = MakeClip({MakeCurve(TEXT("A"), {1.0f}), MakeCurve(TEXT("Extra"), {0.25f})});
	FAnimationDataMappedClip Mapped = MapOk(*this, Source, Profile);
	TestNull(TEXT("unmapped curves are dropped by default"), Find(Mapped, TEXT("Extra")));
	TestTrue(TEXT("missing sources are listed"), Mapped.MissingSources == TArray<FName>{TEXT("Absent")});
	Profile.bPassThroughUnmapped = true;
	Mapped = MapOk(*this, Source, Profile);
	TestTrue(TEXT("pass-through copies unmapped curves"), ValuesNear(Find(Mapped, TEXT("Extra")), {0.25f}));
	TestNull(TEXT("mapped sources do not pass through"), Find(Mapped, TEXT("A")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapperThreads, "AnimationDataTools.Mapper.ThreadAffinity", TestFlags)
bool FMapperThreads::RunTest(const FString& Parameters)
{
	FAnimationDataProfile Profile;
	Profile.Rules = {MakeRule(TEXT("A"), TEXT("T"), 3.0f)};
	const FAnimationDataClip Source = MakeClip({MakeCurve(TEXT("A"), {1.0f, 2.0f})});
	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, SkeletonPath());
	UPoseAsset* Pose = MakePoseAsset(Skeleton);
	const TTuple<EAnimationDataResult, FAnimationDataMappedClip, EAnimationDataResult> Worker = Async(EAsyncExecution::Thread, [&]()
	{
		FAnimationDataMappedClip Mapped;
		FString Error;
		const EAnimationDataResult MapResult = FAnimationDataMapper::Map(Source, Profile, Mapped, Error);
		TArray<FAnimationDataRule> Rules;
		const EAnimationDataResult PoseResult = FAnimationDataMapper::MakeRulesFromPoseAsset(Pose, Rules, Error);
		return MakeTuple(MapResult, Mapped, PoseResult);
	}).Get();
	TestEqual(TEXT("the mapper runs on any thread"), Worker.Get<0>(), EAnimationDataResult::Ok);
	TestTrue(TEXT("with the same result"), ValuesNear(Find(Worker.Get<1>(), TEXT("T")), {3.0f, 6.0f}));
	TestEqual(TEXT("pose assets are read on the game thread only"), Worker.Get<2>(), EAnimationDataResult::WrongThread);
	TArray<FAnimationDataRule> Rules;
	FString Error;
	TestEqual(TEXT("a missing pose asset is refused"), FAnimationDataMapper::MakeRulesFromPoseAsset(nullptr, Rules, Error), EAnimationDataResult::InvalidInput);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapperReflectedCall, "AnimationDataTools.Blueprint.ReflectedCall", TestFlags)
bool FMapperReflectedCall::RunTest(const FString& Parameters)
{
	UFunction* Function = UAnimationDataBlueprintLibrary::StaticClass()->FindFunctionByName(TEXT("MapClip"));
	if (!TestNotNull(TEXT("MapClip is reflected"), Function))
	{
		return false;
	}
	FAnimationDataProfile Profile;
	Profile.Rules = {MakeRule(TEXT("A"), TEXT("T"), 2.0f)};
	FStructOnScope Params(Function);
	*CastField<FStructProperty>(Function->FindPropertyByName(TEXT("Clip")))->ContainerPtrToValuePtr<FAnimationDataClip>(Params.GetStructMemory()) = MakeClip({MakeCurve(TEXT("A"), {0.5f})});
	*CastField<FStructProperty>(Function->FindPropertyByName(TEXT("Profile")))->ContainerPtrToValuePtr<FAnimationDataProfile>(Params.GetStructMemory()) = Profile;
	UAnimationDataBlueprintLibrary::StaticClass()->GetDefaultObject()->ProcessEvent(Function, Params.GetStructMemory());
	const FAnimationDataMappedClip& Mapped = *CastField<FStructProperty>(Function->FindPropertyByName(TEXT("OutClip")))->ContainerPtrToValuePtr<FAnimationDataMappedClip>(Params.GetStructMemory());
	TestEqual(TEXT("reflected result"), *CastField<FEnumProperty>(Function->GetReturnProperty())->ContainerPtrToValuePtr<uint8>(Params.GetStructMemory()), static_cast<uint8>(EAnimationDataResult::Ok));
	TestTrue(TEXT("reflected mapping"), ValuesNear(Find(Mapped, TEXT("T")), {1.0f}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapperBlueprintCompile, "AnimationDataTools.Blueprint.GraphCompiles", TestFlags)
bool FMapperBlueprintCompile::RunTest(const FString& Parameters)
{
	const TArray<UFunction*> Functions = HostTestSupport::BlueprintCallableFunctions(UAnimationDataBlueprintLibrary::StaticClass());
	TestEqual(TEXT("map and pose conversion are callable"), Functions.Num(), 2);
	FString Messages;
	TestTrue(FString::Printf(TEXT("a graph calling every function compiles:\n%s"), *Messages), HostTestSupport::CompileCallsInBlueprint(Functions, Messages));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapperExample, "AnimationDataTools.Example.NormalizedSensorToCurves", TestFlags)
bool FMapperExample::RunTest(const FString& Parameters)
{
	// Example: a raw sensor reading (here a squeeze sensor reporting 200 to 900) drives a curve.
	// The reading is normalised to 0..1 with a gamma as a derived channel before mapping:
	// Value = pow(clamp((Raw - Min) / (Max - Min), 0, 1), Gamma).
	const TArray<float> Raw = {200.0f, 375.0f, 550.0f, 900.0f, 950.0f};
	const float Min = 200.0f, Max = 900.0f, Gamma = 2.0f;
	FAnimationDataCurve Squeeze = MakeCurve(TEXT("Squeeze"), {});
	for (const float Value : Raw)
	{
		Squeeze.Values.Add(FMath::Pow(FMath::Clamp((Value - Min) / (Max - Min), 0.0f, 1.0f), Gamma));
	}
	// The profile normally lives in an asset so many clips share it.
	UAnimationDataProfileAsset* Asset = NewObject<UAnimationDataProfileAsset>(GetTransientPackage());
	Asset->Profile.Rules = {MakeRule(TEXT("Squeeze"), TEXT("HandClose")), MakeRule(TEXT("Squeeze"), TEXT("BrowDown"), 0.3f)};
	const FAnimationDataMappedClip Mapped = MapOk(*this, MakeClip({Squeeze}, 60.0f), Asset->Profile);
	TestTrue(TEXT("hand curve"), ValuesNear(Find(Mapped, TEXT("HandClose")), {0.0f, 0.0625f, 0.25f, 1.0f, 1.0f}));
	TestTrue(TEXT("brow curve"), ValuesNear(Find(Mapped, TEXT("BrowDown")), {0.0f, 0.01875f, 0.075f, 0.3f, 0.3f}));
	return true;
}
