#include "CSBindsRegistry.h"
#include "CSThreadDiagnostics.h"
#include "StructUtils/InstancedStruct.h"

DECLARE_UNREALSHARP_BINDER(Bind_FInstancedStruct)
{
	// Reads a pointer field out of the caller's own 16 bytes: no engine state is touched, so this is an
	// intentional per symbol exemption rather than an oversight.
	const UScriptStruct* GetNativeStruct(const FInstancedStruct& Struct)
	{
		check(&Struct != nullptr);
		return Struct.GetScriptStruct();
	}

	// Constructing an *empty* instanced struct writes the caller's storage and allocates nothing, which is the
	// difference the contract asks for: empty construction carries no payload destructor risk (see NativeDestroy).
	void NativeInit(FInstancedStruct& Struct)
	{
		std::construct_at(&Struct);
	}

	void NativeCopy(FInstancedStruct& Dest, const FInstancedStruct& Src)
	{
		// The source may hold an arbitrary payload, so copying it can allocate and run payload copies.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FInstancedStruct::NativeCopy")))
		{
			return;
		}

		std::construct_at(&Dest, Src);
	}

	void NativeDestroy(FInstancedStruct& Struct)
	{
		// Runs the payload destructor, which can be any struct's destructor.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FInstancedStruct::NativeDestroy")))
		{
			return;
		}

		std::destroy_at(&Struct);
	}

	void InitializeAs(FInstancedStruct& Struct, const UScriptStruct* ScriptStruct, const uint8* StructData)
	{
		// Allocates and constructs the payload.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FInstancedStruct::InitializeAs")))
		{
			return;
		}

		check(ScriptStruct != nullptr);
		Struct.InitializeAs(ScriptStruct, StructData);
	}

	// Returns a pointer into the caller's own struct storage; no engine state is touched (exempt per symbol).
	const uint8* GetMemory(const FInstancedStruct& Struct)
	{
		return Struct.GetMemory();
	}
	
	BIND_UNREALSHARP_FUNCTION(GetNativeStruct)
	BIND_UNREALSHARP_FUNCTION(NativeInit)
	BIND_UNREALSHARP_FUNCTION(NativeCopy)
	BIND_UNREALSHARP_FUNCTION(NativeDestroy)
	BIND_UNREALSHARP_FUNCTION(InitializeAs)
	BIND_UNREALSHARP_FUNCTION(GetMemory)
}
