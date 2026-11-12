#pragma once

#include "CoreMinimal.h"

class UFunction;

namespace HostTestSupport
{
	/**
	 * Builds a transient Blueprint whose one function graph calls every function in Functions, and
	 * compiles it. Every call is wired the way a Blueprint author would wire it: impure calls run in
	 * sequence, each output feeds a member variable, and each by-reference or object-target input
	 * reads a member variable of its own type.
	 *
	 * Returns true when the Blueprint compiles without errors. OutMessages receives the compiler
	 * messages either way. ParentClass defaults to UObject; functions from an editor module need an
	 * editor-only parent such as the editor utility object, as they would in a real project.
	 */
	HOSTTESTSUPPORT_API bool CompileCallsInBlueprint(const TArray<UFunction*>& Functions, FString& OutMessages, UClass* ParentClass = nullptr);

	/** The functions of Class that Blueprints can call, in declaration order. */
	HOSTTESTSUPPORT_API TArray<UFunction*> BlueprintCallableFunctions(const UClass* Class);
}
