using System.Diagnostics;
using System.Runtime.InteropServices;
using UnrealSharp.Core;
using UnrealSharp.Core.Interop;
using UnrealSharp.CoreUObject;
using UnrealSharp.Interop;

namespace UnrealSharp;

[StructLayout(LayoutKind.Sequential)]
public struct FStrongObjectPtr
{
    internal IntPtr NativeObject;
}

public abstract class TStrongObjectPtr : IEquatable<TStrongObjectPtr>, IDisposable
{
    [DebuggerBrowsable(DebuggerBrowsableState.Never)]
    private FStrongObjectPtr _nativePtr;

    private bool _isDisposed;

    protected TStrongObjectPtr(UObject? obj = null)
    {
        Bind_TStrongObjectPtr.CallConstructStrongObjectPtr(ref _nativePtr, obj?.NativeObject ?? IntPtr.Zero);
    }

    ~TStrongObjectPtr()
    {
        Dispose(false);
    }
    
    public bool IsValid => !_isDisposed && _nativePtr.NativeObject != IntPtr.Zero;
    public UObject? Value
    {
        get
        {
            if (!IsValid)
            {
                return null;
            }
            
            IntPtr handle = Bind_UCSManager.CallFindManagedObject(_nativePtr.NativeObject);
            return GCHandleUtilities.GetObjectFromHandlePtr<UObject>(handle);
        }
    }

    public bool Equals(TStrongObjectPtr? other)
    {
        if (other is null)
        {
            return _nativePtr.NativeObject == IntPtr.Zero;
        }
        
        return _nativePtr.NativeObject == other._nativePtr.NativeObject;
    }
    
    public override bool Equals(object? obj)
    {
        return obj is TStrongObjectPtr ptr && Equals(ptr);
    }

    public static bool operator ==(TStrongObjectPtr? a, TStrongObjectPtr? b)
    {
        return a?.Equals(b) ?? b is null;
    }

    public static bool operator !=(TStrongObjectPtr? a, TStrongObjectPtr? b)
    {
        return !(a == b);
    }

    public override int GetHashCode()
    {
        return _nativePtr.NativeObject.GetHashCode();
    }

    public void Dispose()
    {
        Dispose(true);
    }

    private void Dispose(bool disposing)
    {
        if (_isDisposed)
        {
            return;
        }

        // Destroying the pointer releases the object reference the constructor acquired, so a refusal must not
        // clear the pointer: the explicit call reports the failure, the finalizer records it and keeps the
        // reference accounted for instead of leaving a pointer with no owner.
        if (!EngineCallGuard.TryBeginEngineCall($"{nameof(TStrongObjectPtr)}.{nameof(Dispose)}"))
        {
            if (disposing)
            {
                throw new EngineCallRefusedException(
                    "TStrongObjectPtr.Dispose must run on the game thread while no garbage collection is running.");
            }

            return;
        }

        Bind_TStrongObjectPtr.CallDestroyStrongObjectPtr(ref _nativePtr);
        _isDisposed = true;

        if (disposing)
        {
            GC.SuppressFinalize(this);
        }
    }
}

public sealed class TStrongObjectPtr<T>(T? obj = null) : TStrongObjectPtr(obj) where T : UObject
{
    public new T? Value => (T?) base.Value;
    
    public static implicit operator TStrongObjectPtr<T>(T? obj) => new(obj);
}