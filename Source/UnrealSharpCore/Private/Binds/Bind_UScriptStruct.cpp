#include "CSBindsRegistry.h"
#include "CSManager.h"
#include "CSThreadDiagnostics.h"
#include "Types/CSScriptStruct.h"

DECLARE_UNREALSHARP_BINDER(Bind_UScriptStruct)
{
	union FNativeStructData
	{
		std::array<std::byte, 64> SmallStorage;
		void* LargeStorage;
	};
	
	int GetNativeStructSize(const UScriptStruct* ScriptStruct)
	{
		// Reflection lookup on the struct, so it carries its own check.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_UScriptStruct::GetNativeStructSize")))
		{
			return 0;
		}

		if (const UScriptStruct::ICppStructOps* CppStructOps = ScriptStruct->GetCppStructOps(); CppStructOps != nullptr)
		{
			return CppStructOps->GetSize();
		}
		
		return ScriptStruct->GetStructureSize();
	}

	bool NativeCopy(const UScriptStruct* ScriptStruct, void* Src, void* Dest)
	{
		// The struct's own copy can allocate and acquire references, so a refusal must happen before the write.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_UScriptStruct::NativeCopy")))
		{
			return false;
		}

		if (UScriptStruct::ICppStructOps* CppStructOps = ScriptStruct->GetCppStructOps(); CppStructOps != nullptr)
		{
			if (CppStructOps->HasCopy())
			{
				return CppStructOps->Copy(Dest, Src, 1);
			}
		    
	        FMemory::Memcpy(Dest, Src, CppStructOps->GetSize());
	        return true;
	    }
		
		return false;
	}

	bool NativeDestroy(const UScriptStruct* ScriptStruct, void* Struct)
	{
		// An arbitrary struct destructor can release references or free containers.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_UScriptStruct::NativeDestroy")))
		{
			return false;
		}

	    if (UScriptStruct::ICppStructOps* CppStructOps = ScriptStruct->GetCppStructOps(); CppStructOps != nullptr)
		{
			if (CppStructOps->HasDestructor())
			{
				CppStructOps->Destruct(Struct);
			}

	        return true;
		}
		
		return false;
	}

	void AllocateNativeStruct(FNativeStructData& Data, const UScriptStruct* ScriptStruct)
	{
		// Allocates and constructs, so it is refused before either happens; the caller's storage stays untouched.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_UScriptStruct::AllocateNativeStruct")))
		{
			return;
		}

	    if (const int32 NativeSize = GetNativeStructSize(ScriptStruct); NativeSize <= sizeof(FNativeStructData))
	    {
	        ScriptStruct->InitializeStruct(std::addressof(Data.SmallStorage));
	    }
	    else
	    {
	        Data.LargeStorage = FMemory::Malloc(NativeSize);
	        ScriptStruct->InitializeStruct(Data.LargeStorage);       
	    }
	}

	void DeallocateNativeStruct(FNativeStructData& Data, const UScriptStruct* ScriptStruct)
	{
		// Destroys and frees: refusing keeps both the storage and the caller's record of it, instead of leaving a
		// freed pointer that still looks allocated.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_UScriptStruct::DeallocateNativeStruct")))
		{
			return;
		}

	    if (const int32 NativeSize = GetNativeStructSize(ScriptStruct); NativeSize <= sizeof(FNativeStructData))
	    {
	        ScriptStruct->DestroyStruct(std::addressof(Data.SmallStorage));
	    }
	    else
	    {
	        ScriptStruct->DestroyStruct(Data.LargeStorage);    
	        FMemory::Free(Data.LargeStorage);   
	    }
	}

	void* GetStructLocation(FNativeStructData& Data, const UScriptStruct* ScriptStruct)
	{
		// Reads the struct's size through the reflection path, so it is guarded like the other metadata queries.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_UScriptStruct::GetStructLocation")))
		{
			return nullptr;
		}

	    if (const int32 NativeSize = GetNativeStructSize(ScriptStruct); NativeSize <= sizeof(FNativeStructData))
	    {
	        return std::addressof(Data.SmallStorage);
	    }
	    
	    return Data.LargeStorage;
	}

	FGCHandleIntPtr GetManagedStructType(UScriptStruct* ScriptStruct)
	{
		// Touches the manager's type registry, so it is refused before the lookup rather than answering with a
		// handle the caller cannot distinguish from "no managed type".
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_UScriptStruct::GetManagedStructType")))
		{
			return FGCHandleIntPtr();
		}

	    if (const UCSScriptStruct* CSStruct = Cast<UCSScriptStruct>(ScriptStruct); CSStruct != nullptr)
	    {
	        return CSStruct->GetManagedTypeDefinition()->GetTypeGCHandle()->GetHandle();
	    }

	    const UCSManagedAssembly* Assembly = UCSManager::Get().FindOwningAssembly(ScriptStruct);
	    if (Assembly == nullptr)
	    {
	        return FGCHandleIntPtr();
	    }

	    const FCSFieldName FieldName = FCSFieldName::FromNativeBase(ScriptStruct);
	    const TSharedPtr<FCSManagedTypeDefinition> Info = Assembly->FindManagedTypeDefinition(FieldName);
	    if (!Info.IsValid())
	    {
	        return FGCHandleIntPtr();
	    }

	    return Info->GetTypeGCHandle()->GetHandle();   
	}

	BIND_UNREALSHARP_FUNCTION(GetNativeStructSize)
	BIND_UNREALSHARP_FUNCTION(NativeCopy)
	BIND_UNREALSHARP_FUNCTION(NativeDestroy)
	BIND_UNREALSHARP_FUNCTION(AllocateNativeStruct)
	BIND_UNREALSHARP_FUNCTION(DeallocateNativeStruct)
	BIND_UNREALSHARP_FUNCTION(GetStructLocation)
	BIND_UNREALSHARP_FUNCTION(GetManagedStructType)
}
