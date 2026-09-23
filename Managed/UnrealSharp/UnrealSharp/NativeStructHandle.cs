using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using UnrealSharp.Core;
using UnrealSharp.CoreUObject;
using UnrealSharp.Interop;

namespace UnrealSharp;

[InlineArray(64)]
public struct NativeStructHandleData
{
    private byte _data;
}

public sealed class NativeStructHandle : IDisposable
{
    private NativeStructHandleData _nativeStructHandleData;
    private IntPtr _nativeScriptStruct;

    public NativeStructHandle(IntPtr nativeScriptStruct)
    {
        Bind_UScriptStruct.CallAllocateNativeStruct(ref _nativeStructHandleData, nativeScriptStruct);
        _nativeScriptStruct = nativeScriptStruct;
    }
    
    public ref NativeStructHandleData Data => ref _nativeStructHandleData;

    ~NativeStructHandle()
    {
        Dispose(false);
    }

    public void Dispose()
    {
        Dispose(true);
    }

    private void Dispose(bool disposing)
    {
        if (_nativeScriptStruct == IntPtr.Zero)
        {
            return;
        }

        // Refused before the native struct is deallocated: the field is only cleared once the deallocation
        // actually happened, so a refusal cannot leave a struct that looks released but is not.
        if (!EngineCallGuard.TryBeginEngineCall($"{nameof(NativeStructHandle)}.{nameof(Dispose)}"))
        {
            if (disposing)
            {
                throw new EngineCallRefusedException(
                    "NativeStructHandle.Dispose must run on the game thread while no garbage collection is running.");
            }

            return;
        }

        Bind_UScriptStruct.CallDeallocateNativeStruct(ref _nativeStructHandleData, _nativeScriptStruct);
        _nativeScriptStruct = IntPtr.Zero;

        if (disposing)
        {
            GC.SuppressFinalize(this);
        }
    }
}