#pragma once

#include "CoreMinimal.h"

// Case data for the regex replacement test. Pure data, so the contract is explicit.
namespace RegexCases
{
	struct FRegexCase
	{
		const TCHAR* Name;
		const TCHAR* Input;
		const TCHAR* Pattern;
		const TCHAR* Replacement;
		bool bValidPattern;
		const TCHAR* Expected;
	};

	inline TArray<FRegexCase> Cases()
	{
		return {
			// A pattern that matches the empty string at every position.
			{ TEXT("ZeroLengthMatch"), TEXT("bbb"), TEXT("a*"), TEXT("X"), true, TEXT("XbXbXbX") },
			// An unbalanced group does not compile; the input must come back unchanged.
			{ TEXT("InvalidPattern"), TEXT("keep"), TEXT("("), TEXT("X"), false, TEXT("keep") },
			// Two Cyrillic words; the second is replaced.
			{ TEXT("NonAscii"), TEXT("Привет мир"), TEXT("мир"), TEXT("world"), true, TEXT("Привет world") },
			// Replacement text is literal: group references and backslashes are not interpreted.
			{ TEXT("LiteralReplacement"), TEXT("a-b"), TEXT("-"), TEXT("$1\\"), true, TEXT("a$1\\b") },
		};
	}
}
