// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomLibraryBPLibrary.h"
#include "CustomLibrary.h"

UCustomLibraryBPLibrary::UCustomLibraryBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

float UCustomLibraryBPLibrary::CustomLibrarySampleFunction(float Param)
{
	return -1;
}

