using UnrealSharp.Core.Interop;

namespace UnrealSharp.Core;

/// <summary>
/// Decides whether the current thread may touch engine state.
///
/// Loading assemblies, resolving types and creating objects are game thread work, and the engine treats
/// object creation as fatal while a garbage collection holds the object hash tables. Code that resumes on
/// a thread pool thread therefore has to check here (or route the work through the game thread dispatcher)
/// before it calls into the engine, so a violation surfaces as a catchable managed exception instead of a
/// native check that takes the editor down.
/// </summary>
public static class EngineCallGuard
{
    /// <summary>
    /// True when engine state may be touched right now: on the game thread and outside garbage collection.
    /// The answers come from the engine itself, because the managed thread view is not reliable here.
    /// </summary>
    public static bool IsEngineCallSafe =>
        Bind_UCSManager.CallIsOnGameThread() != 0 && Bind_UCSManager.CallIsCollectingGarbage() == 0;

    /// <summary>
    /// Throws a descriptive managed exception when the calling thread must not touch engine state.
    /// </summary>
    public static void EnsureEngineCallAllowed(string what)
    {
        bool OnGameThread = Bind_UCSManager.CallIsOnGameThread() != 0;
        bool CollectingGarbage = Bind_UCSManager.CallIsCollectingGarbage() != 0;

        if (OnGameThread && !CollectingGarbage)
        {
            return;
        }

        throw new System.InvalidOperationException(
            $"{what} must run on the game thread while no garbage collection is running " +
            $"(onGameThread={OnGameThread}, collectingGarbage={CollectingGarbage}, managedThread={System.Environment.CurrentManagedThreadId}). " +
            "Route this call through GameThreadDispatcher.RunAsync before touching engine state.");
    }
}
