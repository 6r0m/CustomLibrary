#include "TextProcessingFragmenterObject.h"
#include "UObject/Package.h"

UTextProcessingFragmenterObject* UTextProcessingFragmenterObject::CreateFragmenter(const FTextProcessingSettings& Settings)
{
	UTextProcessingFragmenterObject* Object = NewObject<UTextProcessingFragmenterObject>(GetTransientPackage());
	Object->Fragmenter = MakeUnique<FTextProcessingFragmenter>(Settings);
	return Object;
}

ETextProcessingResult UTextProcessingFragmenterObject::Append(const FString& Delta, TArray<FString>& OutFragments)
{
	if (!Fragmenter)
	{
		Fragmenter = MakeUnique<FTextProcessingFragmenter>();
	}
	return Fragmenter->Append(Delta, OutFragments);
}

ETextProcessingResult UTextProcessingFragmenterObject::Flush(FString& OutTail)
{
	if (!Fragmenter)
	{
		Fragmenter = MakeUnique<FTextProcessingFragmenter>();
	}
	return Fragmenter->Flush(OutTail);
}

ETextProcessingResult UTextProcessingFragmenterObject::Reset()
{
	if (!Fragmenter)
	{
		Fragmenter = MakeUnique<FTextProcessingFragmenter>();
	}
	return Fragmenter->Reset();
}
