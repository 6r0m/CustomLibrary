#include "BlueprintCompileCheck.h"
#include "ProjectUtilitiesLibrary.h"
#include "RandomUtilitiesLibrary.h"
#include "SceneUtilitiesLibrary.h"
#include "Cases/HelperCases.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GeneralProjectSettings.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodyInstance.h"
#include "UObject/Package.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, UnrealUtilitiesTests)

namespace
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealUtilitiesL1Weighted, "UnrealUtilities.Random.L1WeightedSelection", TestFlags)
bool FUnrealUtilitiesL1Weighted::RunTest(const FString& Parameters)
{
	for (const HelperCases::FWeightedCase& Case : HelperCases::WeightedCases())
	{
		FRandomStream Stream(HelperCases::WeightedSeed);
		int32 Index = 0;
		if (!Case.bValid)
		{
			TestFalse(FString::Printf(TEXT("%s is refused"), Case.Name), URandomUtilitiesLibrary::TrySelectWeightedIndexFromStream(Case.Weights, Stream, Index));
			TestEqual(FString::Printf(TEXT("%s leaves INDEX_NONE"), Case.Name), Index, INDEX_NONE);
			TestFalse(FString::Printf(TEXT("%s is refused without a stream"), Case.Name), URandomUtilitiesLibrary::TrySelectWeightedIndex(Case.Weights, Index));
			continue;
		}
		TArray<int32> Counts;
		Counts.SetNumZeroed(Case.Weights.Num());
		for (int32 Draw = 0; Draw < HelperCases::WeightedDraws; ++Draw)
		{
			if (!URandomUtilitiesLibrary::TrySelectWeightedIndexFromStream(Case.Weights, Stream, Index) || !Counts.IsValidIndex(Index))
			{
				AddError(FString::Printf(TEXT("%s: draw %d failed"), Case.Name, Draw));
				return false;
			}
			++Counts[Index];
		}
		for (int32 Slot = 0; Slot < Case.Weights.Num(); ++Slot)
		{
			if (Case.Weights[Slot] == 0)
			{
				TestEqual(FString::Printf(TEXT("%s: zero-weight index %d is never picked"), Case.Name, Slot), Counts[Slot], 0);
			}
		}
		if (FCString::Strcmp(Case.Name, TEXT("EqualPair")) == 0)
		{
			const double Share = double(Counts[0]) / HelperCases::WeightedDraws;
			TestTrue(FString::Printf(TEXT("EqualPair: share of index 0 is %.3f"), Share), FMath::Abs(Share - 0.5) <= HelperCases::EqualPairTolerance);
		}
		TestTrue(FString::Printf(TEXT("%s works without a stream"), Case.Name), URandomUtilitiesLibrary::TrySelectWeightedIndex(Case.Weights, Index));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealUtilitiesL2LockedAxis, "UnrealUtilities.Scene.L2LockedAxis", TestFlags)
bool FUnrealUtilitiesL2LockedAxis::RunTest(const FString& Parameters)
{
	FVector Axis(1.0, 1.0, 1.0);
	TestFalse(TEXT("a null component is refused"), USceneUtilitiesLibrary::TryGetBodyLockedAxis(nullptr, Axis));
	TestEqual(TEXT("a refused query leaves a zero axis"), Axis, FVector::ZeroVector);

	UBoxComponent* Box = NewObject<UBoxComponent>(GetTransientPackage());
	Box->BodyInstance.SetDOFLock(EDOFMode::XYPlane);
	TestTrue(TEXT("a component with a body succeeds"), USceneUtilitiesLibrary::TryGetBodyLockedAxis(Box, Axis));
	// The helper forwards to the engine's own query; the expected value comes from that same API.
	const FBodyInstance* Body = Box->GetBodyInstance(NAME_None, false);
	TestEqual(TEXT("the axis equals FBodyInstance's answer"), Axis, Body->GetLockedAxis());
	TestEqual(TEXT("an XY-plane lock reports the Z axis"), Axis, FVector(0.0, 0.0, 1.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealUtilitiesL3Frustum, "UnrealUtilities.Scene.L3ViewFrustum", TestFlags)
bool FUnrealUtilitiesL3Frustum::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("a null actor is refused"), USceneUtilitiesLibrary::IsActorCollisionSphereInViewFrustum(GetTransientPackage(), nullptr));
	TestFalse(TEXT("a context without a world is refused"), USceneUtilitiesLibrary::IsActorCollisionSphereInViewFrustum(GetTransientPackage(), GetDefault<AActor>()));

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AActor* Actor = World->SpawnActor<AActor>();
	TestNotNull(TEXT("an actor spawns in the test world"), Actor);
	TestFalse(TEXT("a world without a local player viewport is refused"), USceneUtilitiesLibrary::IsActorCollisionSphereInViewFrustum(Actor, Actor));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealUtilitiesL5ProjectVersion, "UnrealUtilities.Project.L5ProjectVersion", TestFlags)
bool FUnrealUtilitiesL5ProjectVersion::RunTest(const FString& Parameters)
{
	UGeneralProjectSettings* Settings = GetMutableDefault<UGeneralProjectSettings>();
	const FString Previous = Settings->ProjectVersion;
	Settings->ProjectVersion = HelperCases::ProjectVersionForTest();
	const FString Version = UProjectUtilitiesLibrary::GetProjectVersionString();
	Settings->ProjectVersion = Previous;
	TestEqual(TEXT("returns the configured version"), Version, FString(HelperCases::ProjectVersionForTest()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealUtilitiesNativeCalls, "UnrealUtilities.Guard.NativeEngineCallsStayAllowed", TestFlags)
bool FUnrealUtilitiesNativeCalls::RunTest(const FString& Parameters)
{
	// Acceptance control for the naming guard: qualified and member calls to engine APIs that share a
	// name with a retired helper compile and pass the guard.
	const FPlane Ground(FVector::UpVector, 0.0);
	const FVector Hit = FMath::RayPlaneIntersection(FVector(0.0, 0.0, 10.0), FVector(0.0, 0.0, -1.0), Ground);
	TestTrue(TEXT("the engine's ray/plane intersection is usable"), Hit.Equals(FVector::ZeroVector));
	UBoxComponent* Box = NewObject<UBoxComponent>(GetTransientPackage());
	TestEqual(TEXT("the engine's locked-axis query is usable"), Box->BodyInstance.GetLockedAxis(), FVector::ZeroVector);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealUtilitiesReflection, "UnrealUtilities.Blueprint.ReflectedSurface", TestFlags)
bool FUnrealUtilitiesReflection::RunTest(const FString& Parameters)
{
	struct FReflectedFunction
	{
		const UClass* Class;
		const TCHAR* Name;
	};
	const FReflectedFunction Functions[] = {
		{URandomUtilitiesLibrary::StaticClass(), TEXT("TrySelectWeightedIndex")},
		{URandomUtilitiesLibrary::StaticClass(), TEXT("TrySelectWeightedIndexFromStream")},
		{USceneUtilitiesLibrary::StaticClass(), TEXT("IsActorCollisionSphereInViewFrustum")},
		{USceneUtilitiesLibrary::StaticClass(), TEXT("TryGetBodyLockedAxis")},
		{UProjectUtilitiesLibrary::StaticClass(), TEXT("GetProjectVersionString")},
	};
	for (const FReflectedFunction& Entry : Functions)
	{
		const UFunction* Function = Entry.Class->FindFunctionByName(Entry.Name);
		if (!TestNotNull(FString::Printf(TEXT("%s is reflected"), Entry.Name), Function))
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("%s is Blueprint-callable"), Entry.Name), Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure));
		TestTrue(FString::Printf(TEXT("%s is static"), Entry.Name), Function->HasAnyFunctionFlags(FUNC_Static));
	}

	// Invoke one function through reflection, the way a Blueprint graph does.
	UFunction* VersionFunction = UProjectUtilitiesLibrary::StaticClass()->FindFunctionByName(TEXT("GetProjectVersionString"));
	struct FParams
	{
		FString ReturnValue;
	} Params;
	UGeneralProjectSettings* Settings = GetMutableDefault<UGeneralProjectSettings>();
	const FString Previous = Settings->ProjectVersion;
	Settings->ProjectVersion = HelperCases::ProjectVersionForTest();
	UProjectUtilitiesLibrary::StaticClass()->GetDefaultObject()->ProcessEvent(VersionFunction, &Params);
	Settings->ProjectVersion = Previous;
	TestEqual(TEXT("reflected call returns the version"), Params.ReturnValue, FString(HelperCases::ProjectVersionForTest()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealUtilitiesBlueprintCompile, "UnrealUtilities.Blueprint.GraphCompiles", TestFlags)
bool FUnrealUtilitiesBlueprintCompile::RunTest(const FString& Parameters)
{
	TArray<UFunction*> Functions = HostTestSupport::BlueprintCallableFunctions(URandomUtilitiesLibrary::StaticClass());
	Functions.Append(HostTestSupport::BlueprintCallableFunctions(USceneUtilitiesLibrary::StaticClass()));
	Functions.Append(HostTestSupport::BlueprintCallableFunctions(UProjectUtilitiesLibrary::StaticClass()));
	TestEqual(TEXT("every utility is callable from Blueprint"), Functions.Num(), 5);
	FString Messages;
	TestTrue(FString::Printf(TEXT("a graph calling every utility compiles:\n%s"), *Messages), HostTestSupport::CompileCallsInBlueprint(Functions, Messages));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealUtilitiesExample, "UnrealUtilities.Example.WeightedTable", TestFlags)
bool FUnrealUtilitiesExample::RunTest(const FString& Parameters)
{
	// Example: pick a reward tier with a reproducible stream. Zero-weight tiers are unreachable.
	const TArray<int32> TierWeights = { 70, 25, 5, 0 };
	FRandomStream Stream(42);
	int32 Tier = INDEX_NONE;
	TestTrue(TEXT("a tier is picked"), URandomUtilitiesLibrary::TrySelectWeightedIndexFromStream(TierWeights, Stream, Tier));
	TestTrue(TEXT("the zero-weight tier is unreachable"), Tier >= 0 && Tier <= 2);
	return true;
}
