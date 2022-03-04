// 6r0m, MIT liscence


#include "Core/System/CustomBPLibrary.h"
#include "Math/Plane.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EngineSettings/Classes/GeneralProjectSettings.h"

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

const FString UCustomBPLibrary::GetProjectVersion()
{
	const UGeneralProjectSettings& ProjectSettings = *GetDefault<UGeneralProjectSettings>();
	return *ProjectSettings.ProjectVersion;
}

void UCustomBPLibrary::CustomLog(const FString& LogText)
{
	UE_LOG(LogTemp, Display, TEXT("%s"), *LogText);
}
