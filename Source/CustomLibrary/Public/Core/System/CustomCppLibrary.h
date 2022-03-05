// 6r0m, MIT liscence

#pragma once

#include "CoreMinimal.h"

class CUSTOMLIBRARY_API CustomCppLibrary
{
public:
	CustomCppLibrary();
	~CustomCppLibrary();

	//~ Begin Utilities
	template< class T >
	static void CustomShuffleArray(TArray<T>& TargetArray)
	{
		if (TargetArray.Num() > 0)
		{			
			const int32 Idx_Last = TargetArray.Num() - 1;
			for (int32 Idx_Current = 0; Idx_Current <= Idx_Last; ++Idx_Current)
			{
				const int32 Idx_Swap = FMath::RandRange(Idx_Current, Idx_Last);
				if (Idx_Current != Idx_Swap)
				{
					TargetArray.Swap(Idx_Current, Idx_Swap);
				}
			}
		}
	}
	//~ End Utilities

	//~ Begin Memory
	template< class T >
	static FORCEINLINE TArray<T*> GetPointersFromWeak(const TArray<TWeakObjectPtr<T>>& WeakPointers)
	{
		TArray<T*> OutPointers;

		for (auto It_WeakPointer : WeakPointers)
		{
			OutPointers.Add(It_WeakPointer.Get());
		}

		return OutPointers;
	}
	//~ End Memory

};
