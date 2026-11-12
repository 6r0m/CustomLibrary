#include "ProjectUtilitiesLibrary.h"
#include "GeneralProjectSettings.h"

FString UProjectUtilitiesLibrary::GetProjectVersionString()
{
	return GetDefault<UGeneralProjectSettings>()->ProjectVersion;
}
