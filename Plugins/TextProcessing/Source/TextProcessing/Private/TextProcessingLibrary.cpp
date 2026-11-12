#include "TextProcessingLibrary.h"
#include "TextProcessingLog.h"
#include "Internationalization/Regex.h"

namespace
{
	// ICU replacement syntax treats '$' and '\' specially; escaping them keeps the text literal.
	FString EscapeReplacement(const FString& Replacement)
	{
		FString Escaped;
		Escaped.Reserve(Replacement.Len() * 2);
		for (const TCHAR Character : Replacement)
		{
			if (Character == TEXT('\\') || Character == TEXT('$'))
			{
				Escaped.AppendChar(TEXT('\\'));
			}
			Escaped.AppendChar(Character);
		}
		return Escaped;
	}
}

bool UTextProcessingLibrary::ReplaceRegexMatches(const FString& Input, const FString& Pattern, const FString& Replacement, FString& OutText)
{
	const FRegexPattern RegexPattern(Pattern);
	FRegexMatcher Matcher(RegexPattern, Input);
	// The matcher has no region (INDEX_NONE) only when the pattern failed to compile.
	if (Matcher.GetBeginLimit() == INDEX_NONE)
	{
		UE_LOG(LogTextProcessing, Warning, TEXT("ReplaceRegexMatches: the pattern does not compile: %s"), *Pattern);
		OutText = Input;
		return false;
	}
	OutText = Matcher.ReplaceAll(EscapeReplacement(Replacement));
	return true;
}
