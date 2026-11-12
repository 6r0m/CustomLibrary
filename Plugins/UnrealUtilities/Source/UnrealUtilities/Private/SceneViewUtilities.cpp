#include "SceneUtilitiesLibrary.h"
#include "UnrealUtilitiesLog.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "SceneView.h"
#include "UnrealClient.h"

bool USceneUtilitiesLibrary::IsActorCollisionSphereInViewFrustum(const UObject* WorldContextObject, const AActor* Actor)
{
	if (!IsInGameThread())
	{
		UE_LOG(LogUnrealUtilities, Warning, TEXT("IsActorCollisionSphereInViewFrustum must be called on the game thread."));
		return false;
	}
	if (!IsValid(Actor))
	{
		UE_LOG(LogUnrealUtilities, Warning, TEXT("IsActorCollisionSphereInViewFrustum: no actor."));
		return false;
	}
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogUnrealUtilities, Warning, TEXT("IsActorCollisionSphereInViewFrustum: the context object has no world."));
		return false;
	}
	ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController();
	if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport)
	{
		UE_LOG(LogUnrealUtilities, Warning, TEXT("IsActorCollisionSphereInViewFrustum: no local player viewport."));
		return false;
	}

	FViewport* Viewport = LocalPlayer->ViewportClient->Viewport;
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(Viewport, World->Scene, LocalPlayer->ViewportClient->EngineShowFlags)
		.SetRealtimeUpdate(false));
	FVector ViewLocation;
	FRotator ViewRotation;
	const FSceneView* View = LocalPlayer->CalcSceneView(&ViewFamily, ViewLocation, ViewRotation, Viewport);
	if (!View)
	{
		UE_LOG(LogUnrealUtilities, Warning, TEXT("IsActorCollisionSphereInViewFrustum: the local player produced no view."));
		return false;
	}
	return View->ViewFrustum.IntersectSphere(Actor->GetActorLocation(), Actor->GetSimpleCollisionRadius());
}
