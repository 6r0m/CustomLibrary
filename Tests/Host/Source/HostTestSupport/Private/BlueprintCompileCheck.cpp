#include "BlueprintCompileCheck.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/TokenizedMessage.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, HostTestSupport)

namespace HostTestSupport
{
	namespace
	{
		struct FPinVariable
		{
			UK2Node_CallFunction* Call = nullptr;
			FName PinName;
			FName Variable;
			bool bInput = false;
		};

		bool NeedsVariableInput(const UEdGraphPin* Pin, const UFunction* Function)
		{
			if (Pin->Direction != EGPD_Input || Pin->bHidden || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				return false;
			}
			if (Pin->PinName == UEdGraphSchema_K2::PN_Self)
			{
				return !Function->HasAnyFunctionFlags(FUNC_Static);
			}
			// A Blueprint without a world of its own must pass one in explicitly.
			const FString WorldContext = Function->GetMetaData(FBlueprintMetadata::MD_WorldContext);
			return Pin->PinType.bIsReference || (!WorldContext.IsEmpty() && Pin->PinName == FName(*WorldContext));
		}

		bool IsDataOutput(const UEdGraphPin* Pin)
		{
			return Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec;
		}
	}

	TArray<UFunction*> BlueprintCallableFunctions(const UClass* Class)
	{
		TArray<UFunction*> Functions;
		for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if (It->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure))
			{
				Functions.Add(*It);
			}
		}
		Functions.Sort([](const UFunction& A, const UFunction& B) { return A.GetName() < B.GetName(); });
		return Functions;
	}

	bool CompileCallsInBlueprint(const TArray<UFunction*>& Functions, FString& OutMessages, UClass* ParentClass)
	{
		OutMessages.Reset();
		UPackage* Package = GetTransientPackage();
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass ? ParentClass : UObject::StaticClass(), Package,
			MakeUniqueObjectName(Package, UBlueprint::StaticClass(), TEXT("BP_CompileCheck")), BPTYPE_Normal);
		if (!Blueprint)
		{
			OutMessages = TEXT("Could not create a Blueprint");
			return false;
		}
		UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, TEXT("CallEveryFunction"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, Graph, /*bIsUserCreated*/ true, nullptr);

		// Pass 1: place the calls and declare one member variable per pin that needs one. Adding a
		// variable can rebuild the graph's pins, so no pin pointer is kept across this pass.
		TArray<UK2Node_CallFunction*> Calls;
		TArray<FPinVariable> PinVariables;
		for (UFunction* Function : Functions)
		{
			FGraphNodeCreator<UK2Node_CallFunction> CallCreator(*Graph);
			UK2Node_CallFunction* Call = CallCreator.CreateNode(false);
			Call->SetFromFunction(Function);
			CallCreator.Finalize();
			Calls.Add(Call);
			TArray<TPair<FPinVariable, FEdGraphPinType>> Needed;
			for (const UEdGraphPin* Pin : Call->Pins)
			{
				const bool bInput = NeedsVariableInput(Pin, Function);
				if (bInput || IsDataOutput(Pin))
				{
					FEdGraphPinType VariableType = Pin->PinType;
					VariableType.bIsReference = false;
					VariableType.bIsConst = false;
					FPinVariable Entry;
					Entry.Call = Call;
					Entry.PinName = Pin->PinName;
					Entry.bInput = bInput;
					Needed.Emplace(Entry, VariableType);
				}
			}
			for (TPair<FPinVariable, FEdGraphPinType>& Entry : Needed)
			{
				Entry.Key.Variable = FName(*FString::Printf(TEXT("Var%d"), PinVariables.Num()));
				FBlueprintEditorUtils::AddMemberVariable(Blueprint, Entry.Key.Variable, Entry.Value);
				PinVariables.Add(Entry.Key);
			}
		}

		// Pass 2: wire. Impure calls and variable sets run in declaration order.
		TArray<UK2Node_FunctionEntry*> Entries;
		Graph->GetNodesOfClass(Entries);
		if (Entries.Num() != 1)
		{
			OutMessages = TEXT("The function graph has no single entry node");
			return false;
		}
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		TArray<FString> WiringErrors;
		UEdGraphPin* LastThen = Entries[0]->FindPin(UEdGraphSchema_K2::PN_Then);
		auto Chain = [&](UK2Node* Node, const FString& Label)
		{
			if (!Schema->TryCreateConnection(LastThen, Node->GetExecPin()))
			{
				WiringErrors.Add(Label + TEXT(": exec"));
			}
			LastThen = Node->GetThenPin();
		};

		for (UK2Node_CallFunction* Call : Calls)
		{
			const FString CallName = Call->GetTargetFunction() ? Call->GetTargetFunction()->GetName() : TEXT("?");
			if (!Call->IsNodePure())
			{
				Chain(Call, CallName);
			}
			for (const FPinVariable& Entry : PinVariables)
			{
				if (Entry.Call != Call)
				{
					continue;
				}
				UEdGraphPin* Pin = Call->FindPin(Entry.PinName);
				const FString Label = CallName + TEXT(".") + Entry.PinName.ToString();
				if (!Pin)
				{
					WiringErrors.Add(Label + TEXT(": pin not found"));
					continue;
				}
				if (Entry.bInput)
				{
					FGraphNodeCreator<UK2Node_VariableGet> GetCreator(*Graph);
					UK2Node_VariableGet* Get = GetCreator.CreateNode(false);
					Get->VariableReference.SetSelfMember(Entry.Variable);
					GetCreator.Finalize();
					if (!Schema->TryCreateConnection(Get->GetValuePin(), Pin))
					{
						WiringErrors.Add(Label + TEXT(": input"));
					}
				}
				else
				{
					FGraphNodeCreator<UK2Node_VariableSet> SetCreator(*Graph);
					UK2Node_VariableSet* Set = SetCreator.CreateNode(false);
					Set->VariableReference.SetSelfMember(Entry.Variable);
					SetCreator.Finalize();
					Chain(Set, Label);
					if (!Schema->TryCreateConnection(Pin, Set->FindPin(Entry.Variable)))
					{
						WiringErrors.Add(Label + TEXT(": output"));
					}
				}
			}
		}

		FCompilerResultsLog Results;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
		for (const TSharedRef<FTokenizedMessage>& Message : Results.Messages)
		{
			OutMessages += Message->ToText().ToString() + TEXT("\n");
		}
		for (const FString& Error : WiringErrors)
		{
			OutMessages += TEXT("Could not wire ") + Error + TEXT("\n");
		}
		return WiringErrors.IsEmpty() && Results.NumErrors == 0 && Blueprint->Status != BS_Error;
	}
}
