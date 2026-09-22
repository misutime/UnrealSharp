using System.Runtime.InteropServices;
using UnrealSharp.Core;
using UnrealSharp.Interop;

namespace UnrealSharp;

/// <summary>
/// Hands work over to the game thread without requiring a world context object.
///
/// Unreal objects may only be created and mutated on the game thread. Any code that resumes on a
/// thread pool thread (i.e. after an await without a game thread synchronization context) must
/// route its engine-facing work through here: creating an object off the game thread is illegal,
/// and the engine treats it as fatal when a garbage collection holds the object hash tables.
/// Whether the caller already sits on the game thread is decided natively, where that question can
/// be answered reliably. Note that the delegate is invoked with a null world context, so it must not
/// assume a current world.
/// </summary>
public static class GameThreadDispatcher
{
    /// <summary>
    /// True when engine state may be touched right now: on the game thread and outside garbage collection.
    /// </summary>
    public static bool IsEngineCallSafe => EngineCallGuard.IsEngineCallSafe;

    /// <summary>
    /// Fails with a descriptive managed exception when the calling thread must not touch engine state.
    /// </summary>
    public static void EnsureEngineCallAllowed(string what) => EngineCallGuard.EnsureEngineCallAllowed(what);

    public static Task RunAsync(Action action)
    {
        TaskCompletionSource completion = new(TaskCreationOptions.RunContinuationsAsynchronously);

        GCHandle callbackHandle = GCHandle.Alloc(new Action(() =>
        {
            try
            {
                action();
                completion.SetResult();
            }
            catch (Exception exception)
            {
                completion.SetException(exception);
            }
        }));

        Bind_Async.CallRunOnGameThread(GCHandle.ToIntPtr(callbackHandle));

        return completion.Task;
    }
}
