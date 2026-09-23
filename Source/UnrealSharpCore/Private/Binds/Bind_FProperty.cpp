#include "CSBindsRegistry.h"
#include "CSThreadDiagnostics.h"
#include "INotifyFieldValueChanged.h"

DECLARE_UNREALSHARP_BINDER(Bind_FProperty)
{
	FProperty* GetNativePropertyFromName(UStruct* Struct, const char* PropertyName)
	{
		// Reflection lookup: it walks the struct's field chain, which hot reload can reinstance underneath a
		// caller on another thread.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FProperty::GetNativePropertyFromName")))
		{
			return nullptr;
		}

		FProperty* Property = FindFProperty<FProperty>(Struct, PropertyName);
		return Property;
	}

	// Metadata reads dereference the cached property. On refusal they answer INDEX_NONE instead of a plausible
	// value: zero is a legitimate offset and a legitimate array dimension is never negative, so a negative
	// answer can only mean "refused" and the managed side rejects it. Silently returning zero would turn a
	// refusal into a wrong memory access.
	int32 GetPropertyOffset(FProperty* Property)
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FProperty::GetPropertyOffset")))
		{
			return INDEX_NONE;
		}

		return Property->GetOffset_ForInternal();
	}

	int32 GetSize(FProperty* Property)
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FProperty::GetSize")))
		{
			return INDEX_NONE;
		}

		return Property->GetSize();
	}

	int32 GetArrayDim(FProperty* Property)
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FProperty::GetArrayDim")))
		{
			return INDEX_NONE;
		}

		return Property->ArrayDim;
	}

	void DestroyValue(FProperty* Property, void* Value)
	{
		// Destroying a value can run an arbitrary destructor (release an object reference, free a container), so
		// it follows the same rule as the other lifecycle entries: refuse before anything happens.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FProperty::DestroyValue")))
		{
			return;
		}

		Property->DestroyValue(Value);
	}

	void DestroyValue_InContainer(FProperty* Property, void* Value)
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FProperty::DestroyValue_InContainer")))
		{
			return;
		}

		Property->DestroyValue_InContainer(Value);
	}

	void InitializeValue(FProperty* Property, void* Value)
	{
		// Constructing a value can allocate engine state inside it, so it is refused before the write.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FProperty::InitializeValue")))
		{
			return;
		}

		Property->InitializeValue(Value);
	}

	// Pure value read and compare: no lifecycle, no allocation, no engine container - the property dispatch is
	// what a development build covers (Tier B, the generated accessor's first dereference). Deliberately no Tier A
	// guard here, because the generated accessors call this once per property access and the mechanism must not
	// charge the hot path twice (see the Tier A/B assignment in the plan of record).
	bool Identical(const FProperty* Property, void* ValueA, void* ValueB)
	{
		bool bIsIdentical = Property->Identical(ValueA, ValueB);
		return bIsIdentical;
	}

	void GetInnerFields(FProperty* SetProperty, TArray<FField*>* OutFields)
	{
		// Writes into an engine container that the caller handed over.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FProperty::GetInnerFields")))
		{
			return;
		}

		SetProperty->GetInnerFields(*OutFields);
	}

	// Pure value read of the caller's buffer; see Identical.
	uint32 GetValueTypeHash(FProperty* Property, void* Source)
	{
		return Property->GetValueTypeHash(Source);
	}

	bool HasAnyPropertyFlags(FProperty* Property, EPropertyFlags FlagsToCheck)
	{
		return Property->HasAnyPropertyFlags(FlagsToCheck);
	}

	bool HasAllPropertyFlags(FProperty* Property, EPropertyFlags FlagsToCheck)
	{
		return Property->HasAllPropertyFlags(FlagsToCheck);
	}

	void CopySingleValue(FProperty* Property, void* Dest, void* Src)
	{
		// Copying is not a plain memcpy: for properties with lifecycle it releases what the destination held and
		// acquires what the source holds.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FProperty::CopySingleValue")))
		{
			return;
		}

		Property->CopySingleValue(Dest, Src);
	}

	// Pure value read / write of the caller's buffer; see Identical.
	void GetValue_InContainer(FProperty* Property, void* Container, void* OutValue)
	{
		Property->GetValue_InContainer(Container, OutValue);
	}

	void SetValue_InContainer(FProperty* Property, void* Container, void* Value)
	{
		Property->SetValue_InContainer(Container, Value);
	}

	uint8 GetBoolPropertyFieldMaskFromName(UStruct* InStruct, const char* InPropertyName)
	{
		// Reflection lookup, so it carries its own check like the other metadata queries.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FProperty::GetBoolPropertyFieldMaskFromName")))
		{
			return 0;
		}

		FBoolProperty* Property = FindFProperty<FBoolProperty>(InStruct, InPropertyName);
		if (!Property)
		{
			return 0;
		}

		return Property->GetFieldMask();
	}

	int32 GetPropertyOffsetFromName(UStruct* InStruct, const char* InPropertyName)
	{
		FProperty* FoundProperty = GetNativePropertyFromName(InStruct, InPropertyName);
		if (!FoundProperty)
		{
			return -1;
		}
		
		return GetPropertyOffset(FoundProperty);
	}

	int32 GetPropertyArrayDimFromName(UStruct* InStruct, const char* PropertyName)
	{
		FProperty* Property = GetNativePropertyFromName(InStruct, PropertyName);

		// Same answer as a refusal: without a property there is no dimension to report, and dereferencing the
		// miss would crash.
		if (Property == nullptr)
		{
			return INDEX_NONE;
		}

		return GetArrayDim(Property);
	}

	void BroadcastFieldValueChanged(UObject* Object, FProperty* Property)
	{
		// Notifies engine listeners, so it mutates engine state and runs arbitrary listeners; it is refused
		// before anything is broadcast.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FProperty::BroadcastFieldValueChanged")))
		{
			return;
		}

		TScriptInterface<INotifyFieldValueChanged> NotifyFieldSelf = Object;
		FName FieldName = Property->GetFName();
		if (NotifyFieldSelf.GetObject() != nullptr && NotifyFieldSelf.GetInterface() != nullptr && FieldName.IsValid())
		{
			const UE::FieldNotification::FFieldId FieldId = NotifyFieldSelf->GetFieldNotificationDescriptor().GetField(Object->GetClass(), FieldName);
			if (FieldId.IsValid())
			{
				NotifyFieldSelf->BroadcastFieldValueChanged(FieldId);
			}
		}
	}
	
	BIND_UNREALSHARP_FUNCTION(GetNativePropertyFromName)
	BIND_UNREALSHARP_FUNCTION(GetPropertyOffset)
	BIND_UNREALSHARP_FUNCTION(GetSize)
	BIND_UNREALSHARP_FUNCTION(GetArrayDim)
	BIND_UNREALSHARP_FUNCTION(DestroyValue)
	BIND_UNREALSHARP_FUNCTION(DestroyValue_InContainer)
	BIND_UNREALSHARP_FUNCTION(InitializeValue)
	BIND_UNREALSHARP_FUNCTION(Identical)
	BIND_UNREALSHARP_FUNCTION(GetInnerFields)
	BIND_UNREALSHARP_FUNCTION(GetValueTypeHash)
	BIND_UNREALSHARP_FUNCTION(HasAnyPropertyFlags)
	BIND_UNREALSHARP_FUNCTION(HasAllPropertyFlags)
	BIND_UNREALSHARP_FUNCTION(CopySingleValue)
	BIND_UNREALSHARP_FUNCTION(GetValue_InContainer)
	BIND_UNREALSHARP_FUNCTION(SetValue_InContainer)
	BIND_UNREALSHARP_FUNCTION(GetBoolPropertyFieldMaskFromName)
	BIND_UNREALSHARP_FUNCTION(GetPropertyOffsetFromName)
	BIND_UNREALSHARP_FUNCTION(GetPropertyArrayDimFromName)
	BIND_UNREALSHARP_FUNCTION(BroadcastFieldValueChanged)
}