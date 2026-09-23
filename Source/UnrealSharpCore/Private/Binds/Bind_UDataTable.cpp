#include "CSBindsRegistry.h"
#include "CSThreadDiagnostics.h"

DECLARE_UNREALSHARP_BINDER(Bind_UDataTable)
{
	uint8* GetRow(const UDataTable* DataTable, FName RowName)
	{
		// The answer is borrowed row memory, not a copy: handing it out from the wrong thread would let managed
		// code cache a pointer into a row table that the game thread can reallocate.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_UDataTable::GetRow")))
		{
			return nullptr;
		}

		if (!IsValid(DataTable))
		{
			return nullptr;
		}

		return DataTable->FindRowUnchecked(RowName);
	}
	
	BIND_UNREALSHARP_FUNCTION(GetRow)
}
