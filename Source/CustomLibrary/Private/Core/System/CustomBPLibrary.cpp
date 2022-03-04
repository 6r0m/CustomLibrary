// 6r0m, MIT liscence


#include "Core/System/CustomBPLibrary.h"
#include "Math/Plane.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EngineSettings/Classes/GeneralProjectSettings.h"

DEFINE_LOG_CATEGORY(LogCustomBPLibrary);

void UCustomBPLibrary::CustomLog(const FString& LogText)
{
	UE_LOG(LogTemp, Display, TEXT("%s"), *LogText);
}

const FVector UCustomBPLibrary::RayPlaneIntersection(const FVector& RayOrigin, const FVector& RayDirection, const FPlane& Plane)
{
	return FMath::RayPlaneIntersection(RayOrigin, RayDirection, Plane);
}

const FVector UCustomBPLibrary::GetLockedAxis(const UPrimitiveComponent* PrimitiveComponent)
{
	const FBodyInstance* RootBI = PrimitiveComponent->GetBodyInstance(NAME_None, false);

	if (RootBI == NULL || PrimitiveComponent->IsPendingKill())
	{
		return FVector::ZeroVector;
	}

	return RootBI->GetLockedAxis();
}

const bool UCustomBPLibrary::IsActorInFrustrum(const UObject* _WorldContextObject, const AActor* _Actor, FVector& OutViewLocation, FRotator& OutViewRotation)
{
	if (!_WorldContextObject)
	{
		UE_LOG(LogCustomBPLibrary, Error, TEXT("%s -- WorldContextObject isn't valid"), *FString(__FUNCTION__));
		return false;
	}

	const UWorld* World = GEngine->GetWorldFromContextObjectChecked(_WorldContextObject);;

	ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController();
	const bool bPlayerViewportValid = LocalPlayer != nullptr && LocalPlayer->ViewportClient != nullptr && LocalPlayer->ViewportClient->Viewport;	
	if (!bPlayerViewportValid)
	{
		UE_LOG(LogCustomBPLibrary, Error, TEXT("%s -- PlayerViewport isn't valid"), *FString(__FUNCTION__));
		return false;
	}

	FSceneViewFamilyContext SceneViewFamilyContext(FSceneViewFamily::ConstructionValues(LocalPlayer->ViewportClient->Viewport, World->Scene,
		LocalPlayer->ViewportClient->EngineShowFlags).SetRealtimeUpdate(false));

	const FSceneView* SceneView = LocalPlayer->CalcSceneView(&SceneViewFamilyContext, OutViewLocation, OutViewRotation, LocalPlayer->ViewportClient->Viewport);
	if (SceneView == nullptr) 
	{
		UE_LOG(LogCustomBPLibrary, Error, TEXT("%s -- SceneView isn't valid"), *FString(__FUNCTION__));
		return false;
	}
	
	return SceneView->ViewFrustum.IntersectSphere(_Actor->GetActorLocation(), _Actor->GetSimpleCollisionRadius());
}

const FString UCustomBPLibrary::GetProjectVersion()
{
	const UGeneralProjectSettings& ProjectSettings = *GetDefault<UGeneralProjectSettings>();
	return *ProjectSettings.ProjectVersion;
}

const bool UCustomBPLibrary::GetCmdParameter(const FString& InKey, FString& OutValue)
{
	const FString CommandLine = UKismetSystemLibrary::GetCommandLine();
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Params;

	UKismetSystemLibrary::ParseCommandLine(CommandLine, Tokens, Switches, Params);

	if (Params.Contains(InKey))
	{
		OutValue = *(Params.Find(InKey));
		return true;
	}
	else
	{
		return false;
	}
}

const FString UCustomBPLibrary::RegexReplace(const FString& InString, const FString& Replacement, const FString& Pattern /*= "<[^>]*>"*/)
{
	FString Result;
	const int32 StringLength = InString.Len();
	
	// Prepare Parser
	int32 Idx_LastMatchEnd = 0;
	FRegexPattern RegexPattern(Pattern);
	FRegexMatcher RegexMatcher(RegexPattern, InString);

	// Parse
	while (RegexMatcher.FindNext())
	{
		const int32 Idx_MatchBegin = RegexMatcher.GetMatchBeginning();
		const int32 Idx_MatchEnd = RegexMatcher.GetMatchEnding();

		for (int32 Idx_MatchingChar = Idx_LastMatchEnd; Idx_MatchingChar < Idx_MatchBegin; Idx_MatchingChar++)
		{
			Result += InString[Idx_MatchingChar];
		}
		Result += Replacement;

		RegexMatcher.SetLimits(Idx_MatchEnd, StringLength);
		Idx_LastMatchEnd = Idx_MatchEnd;
	}
	
	// Add unmatching rest of the Input String to the result
	for (int32 Idx_RestChar = Idx_LastMatchEnd; Idx_RestChar < StringLength; Idx_RestChar++)
	{
		Result += InString[Idx_RestChar];
	}

	return Result;
}
