#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SceneUtilitiesLibrary.generated.h"

class AActor;
class UPrimitiveComponent;

/** Validated scene and physics queries for actors and components. */
UCLASS()
class UNREALUTILITIES_API USceneUtilitiesLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Approximate visibility test: the actor's simple collision sphere against the view frustum of the
	 * world's first local player. It ignores occlusion and rendered bounds. Returns false, and logs
	 * why, when there is no world, local player, viewport or actor.
	 */
	UFUNCTION(BlueprintCallable, Category = "Unreal Utilities|View", meta = (WorldContext = "WorldContextObject"))
	static bool IsActorCollisionSphereInViewFrustum(const UObject* WorldContextObject, const AActor* Actor);

	/**
	 * Reads the locked axis of the component's root body. Returns false and a zero vector when the
	 * component or its body instance is missing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Unreal Utilities|Physics")
	static bool TryGetBodyLockedAxis(const UPrimitiveComponent* Component, FVector& OutAxis);
};
