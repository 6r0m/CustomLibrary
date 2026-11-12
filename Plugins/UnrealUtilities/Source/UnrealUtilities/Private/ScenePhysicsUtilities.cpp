#include "SceneUtilitiesLibrary.h"
#include "UnrealUtilitiesLog.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/BodyInstance.h"

bool USceneUtilitiesLibrary::TryGetBodyLockedAxis(const UPrimitiveComponent* Component, FVector& OutAxis)
{
	OutAxis = FVector::ZeroVector;
	if (!IsValid(Component))
	{
		UE_LOG(LogUnrealUtilities, Warning, TEXT("TryGetBodyLockedAxis: no component."));
		return false;
	}
	const FBodyInstance* Body = Component->GetBodyInstance(NAME_None, false);
	if (!Body)
	{
		UE_LOG(LogUnrealUtilities, Warning, TEXT("TryGetBodyLockedAxis: %s has no body instance."), *Component->GetName());
		return false;
	}
	OutAxis = Body->GetLockedAxis();
	return true;
}
