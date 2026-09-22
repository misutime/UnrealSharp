using UnrealSharp.Core.Interop;

namespace UnrealSharp.Core;

public static class NativeReflectionHelper
{
	private enum ECSFieldType : byte
	{
		Unknown,
		Class,
		Struct,
		Enum,
		Interface,
		Delegate
	}

	private static ECSFieldType GetFieldType(Type type)
	{
		if (type.IsInterface)
		{
			return ECSFieldType.Interface;
		}

		if (type.IsEnum)
		{
			return ECSFieldType.Enum;
		}

		if (typeof(Delegate).IsAssignableFrom(type))
		{
			return ECSFieldType.Delegate;
		}
		
		if (type.IsValueType)
		{
			return ECSFieldType.Struct;
		}
		
		if (type.IsClass)
		{
			return ECSFieldType.Class;
		}

		return ECSFieldType.Unknown;
	}
	
	public static IntPtr GetNativeField<T>() => GetNativeField(typeof(T));
	public static IntPtr GetNativeField(Type type) => GetNativeField(type, type.Name);

	public static IntPtr GetNativeField(Type type, string sourceName)
	{
		// Resolving the field loads assemblies and compiles managed types, so it is game thread work.
		EngineCallGuard.EnsureEngineCallAllowed(nameof(GetNativeField));

		IntPtr NativeField = Bind_UCoreUObject.CallGetNativeField(type.Assembly.GetName().Name!, type.Namespace, sourceName,
			(byte)GetFieldType(type));

		if (NativeField == IntPtr.Zero)
		{
			// Generated bindings cache this pointer in their static constructor and then call native
			// functions with it. Returning zero would let them call in with a null pointer and trip a
			// native check, which kills the editor; failing the constructor keeps the error catchable
			// (note that .NET then poisons this type's static constructor for the rest of the process).
			throw new InvalidOperationException(
				$"Failed to resolve the native field for '{type.FullName}' (assembly '{type.Assembly.GetName().Name}').");
		}

		return NativeField;
	}
}