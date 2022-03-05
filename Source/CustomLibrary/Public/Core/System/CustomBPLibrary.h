// 6r0m, MIT liscence

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "CustomBPLibrary.generated.h"

/* 
*	Function library class.
*	Each function in it is expected to be static and represents blueprint node that can be called in any blueprint.
*
*	When declaring function you can define metadata for the node. Key function specifiers will be BlueprintPure and BlueprintCallable.
*	BlueprintPure - means the function does not affect the owning object in any way and thus creates a node without Exec pins.
*	BlueprintCallable - makes a function which can be executed in Blueprints - Thus it has Exec pins.
*	DisplayName - full name of the node, shown when you mouse over the node and in the blueprint drop down menu.
*				Its lets you name the node using characters not allowed in C++ function names.
*	CompactNodeTitle - the word(s) that appear on the node.
*	Keywords -	the list of keywords that helps you to find node when you search for it using Blueprint drop-down menu. 
*				Good example is "Print String" node which you can find also by using keyword "log".
*	Category -	the category your node will be under in the Blueprint drop-down menu.
*/

DECLARE_LOG_CATEGORY_EXTERN(LogCustomBPLibrary, Log, All);

UCLASS()
class UCustomBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	//~ Begin Debug
	UFUNCTION(BlueprintCallable, Category = "CustomBFLibrary|Debug")
	static void CustomLog(const FString& LogText);
	//~ End Debug

	//~ Begin Math
	UFUNCTION(BlueprintPure, Category = "CustomBFLibrary|Math", Meta = (ToolTip = 
		"Find Random Index with given Weights Array. Useful to give greater probability to certain indexes."))
	static const int32 RandomWeightIndex(const TArray<int32>& WeightsArray);
	
	UFUNCTION(BlueprintCallable, Category = "CustomBFLibrary|Math")
	static const FVector RayPlaneIntersection(const FVector& RayOrigin, const FVector& RayDirection, const FPlane& Plane);
	//~ End Math

	//~ Begin Physx
	UFUNCTION(BlueprintCallable, Category = "CustomBFLibrary|Physx")
	static const FVector GetLockedAxis(const UPrimitiveComponent* PrimitiveComponent);
	//~ End Physx

	//~ Begin Rendering
	UFUNCTION(BlueprintCallable, Category = "CustomBFLibrary|Rendering", meta = (WorldContext = "_WorldContextObject", 
		ToolTip = "Check if some actor is in frustrum"))
	static const bool IsActorInFrustrum(const UObject* _WorldContextObject, const AActor* _Actor, FVector& OutViewLocation, FRotator& OutViewRotation);
	//~ End Rendering

	//~ Begin Settings
	UFUNCTION(BlueprintPure, Category = "CustomBFLibrary|Settings")
	static const FString GetProjectVersion();
	//~ End Settings

	//~ Begin Utilities
	UFUNCTION(BlueprintCallable, Category = "CustomBFLibrary|Utilities", meta = (Keywords = "Cmd, Command Line, Parser"))
	static const bool GetCmdParameter(const FString& InKey, FString& OutValue);

	UFUNCTION(BlueprintPure, Category = "CustomBFLibrary|Utilities", Meta = (DisplayName = "Regex Replace",
		ToolTip = "Regex Replace from a pattern with default pattern for HTML tags. Leave Replacement empty if you need just delete finded strings by pattern."))
	static const FString RegexReplace(const FString& InString, const FString& Replacement, const FString& Pattern = "<[^>]*>");
	//~ End Utilities

	//~ Begin XR
	UFUNCTION(BlueprintPure, Category = "CustomBFLibrary|XR")
	static void GetHMDViewOffset(const bool RightEye, const float WorldToMeters, FVector& OutViewLocation, FRotator& OutViewRotation);
	
	UFUNCTION(BlueprintPure, Category = "CustomBFLibrary|XR")
	static void GetHMDProjectionMatix(const bool RightEye, FMatrix& Matrix);
	//~ End XR
};
