using UnrealSharp.Core.Interop;

namespace UnrealSharp.Core;

/// <summary>
/// Raised when managed code asked to touch engine state from a thread or from a state where the engine does
/// not allow it. It derives from <see cref="System.InvalidOperationException"/> so existing handlers keep
/// working, and it is distinguishable so call sites that must not let an exception escape (finalizers, native
/// to managed callbacks) can catch exactly the refusal and report it instead of unwinding into native code.
/// </summary>
public class EngineCallRefusedException : System.InvalidOperationException
{
    public EngineCallRefusedException(string message) : base(message)
    {
    }
}

/// <summary>
/// Decides whether the current thread may touch engine state.
///
/// Loading assemblies, resolving types and creating objects are game thread work, and the engine treats
/// object creation as fatal while a garbage collection holds the object hash tables. Code that resumes on
/// a thread pool thread therefore has to check here (or route the work through the game thread dispatcher)
/// before it calls into the engine, so a violation surfaces as a catchable managed exception instead of a
/// native check that takes the process down.
///
/// The state comes from the engine itself, in a single packed query: the managed thread view is not reliable
/// here, and one round trip per call site keeps the guard cheap enough to sit on hot paths. The value is a
/// snapshot, not a lock, and the garbage collection bits must never be cached.
/// </summary>
public static class EngineCallGuard
{
    private const int InGameThreadBit = 1;
    private const int CollectingGarbageBit = 2;
    private const int LockingUObjectHashTablesBit = 4;

    /// <summary>Packed state: bit0 = game thread, bit1 = garbage collection running, bit2 = GC locking the object hash tables.</summary>
    public static int GetEngineCallState() => Bind_UCSManager.CallGetEngineCallState();

    /// <summary>Number of refusals the native guards recorded (Tier A). Diagnostic only.</summary>
    public static int GetRefusalCount() => Bind_UCSManager.CallGetThreadRefusalCount();

    /// <summary>Number of violations caught at boundaries where an escaping exception would kill the process.</summary>
    public static int GetBoundaryCatchCount() => Bind_UCSManager.CallGetThreadBoundaryCatchCount();

    /// <summary>
    /// True when engine state may be touched right now: on the game thread and outside garbage collection.
    /// Conservative on purpose - any garbage collection is treated as unsafe, not only the hash table lock.
    /// </summary>
    public static bool IsEngineCallSafe
    {
        get
        {
            int state = GetEngineCallState();
            return (state & InGameThreadBit) != 0 && (state & (CollectingGarbageBit | LockingUObjectHashTablesBit)) == 0;
        }
    }

    /// <summary>
    /// Throws a descriptive managed exception when the calling thread must not touch engine state.
    ///
    /// A refusal raised here happens before any native call, so no native guard observes it. It therefore
    /// reports itself, otherwise the refusal count would only ever show the cases that slipped past the check.
    /// </summary>
    public static void EnsureEngineCallAllowed(string what)
    {
        int state = GetEngineCallState();
        bool onGameThread = (state & InGameThreadBit) != 0;
        bool collectingGarbage = (state & CollectingGarbageBit) != 0;
        bool lockingHashTables = (state & LockingUObjectHashTablesBit) != 0;

        if (onGameThread && !collectingGarbage && !lockingHashTables)
        {
            return;
        }

        ReportRefusal(what);

        throw new EngineCallRefusedException(
            $"{what} must run on the game thread while no garbage collection is running " +
            $"(onGameThread={onGameThread}, collectingGarbage={collectingGarbage}, lockingHashTables={lockingHashTables}, " +
            $"managedThread={System.Environment.CurrentManagedThreadId}). " +
            "Route this call through GameThreadDispatcher.RunAsync before touching engine state.");
    }

    /// <summary>
    /// Non-throwing judgement for boundaries where an escaping exception would take the process down (a managed
    /// finalizer, a native to managed callback): it decides whether engine state may be touched, records the
    /// refusal and the boundary catch when it may not, and never throws.
    ///
    /// Callers must leave their state intact when this answers false. Refusing must never produce a half state -
    /// for a reference counted resource that means neither releasing without clearing nor clearing without
    /// releasing, and for a delegate registration it means the registration is not made at all.
    /// </summary>
    public static bool TryBeginEngineCall(string what)
    {
        if (IsEngineCallSafe)
        {
            return true;
        }

        ReportRefusal(what);
        ReportBoundaryCatch(what);
        return false;
    }

    /// <summary>
    /// Reports a result that can only mean the engine refused the call. Entry points whose null would be cached
    /// or dereferenced by the caller must never hand a null through, so the wrapper turns it into the refusal
    /// exception here. Runs on the native refusal path only, so it costs nothing on legal calls.
    /// </summary>
    public static void EnsureNotNullResult(IntPtr result, string what)
    {
        if (result != IntPtr.Zero)
        {
            return;
        }

        ReportRefusal(what);

        throw new EngineCallRefusedException(
            $"{what} returned a null result. The engine refuses this call outside the game thread or while a " +
            "garbage collection is running, and a null here has no legal meaning for the caller.");
    }

    /// <summary>
    /// Same as <see cref="EnsureNotNullResult"/> for entry points that answer a refusal with a negative integer
    /// where no legitimate answer is negative.
    /// </summary>
    public static void EnsureNotNegativeResult(int result, string what)
    {
        if (result >= 0)
        {
            return;
        }

        ReportRefusal(what);

        throw new EngineCallRefusedException(
            $"{what} returned the negative result {result}. The engine refuses this call outside the game " +
            "thread or while a garbage collection is running, and a negative answer is never a legal value here.");
    }

    /// <summary>
    /// Best effort refusal accounting. A diagnostic must never be the reason a refusal goes unreported or a
    /// call site dies, so a failure to record is swallowed.
    /// </summary>
    private static void ReportRefusal(string what)
    {
        try
        {
            Bind_UCSManager.CallRecordManagedRefusal(what);
        }
        catch
        {
        }
    }

    /// <summary>
    /// Records a violation observed inside a boundary where exceptions must not escape (managed finalizer,
    /// native to managed callback). Never throws and never logs through the managed bridge: an exception here
    /// would take the process down instead of reporting a diagnosable violation.
    /// </summary>
    public static void ReportBoundaryCatch(string what)
    {
        try
        {
            Bind_UCSManager.CallReportBoundaryCatch(what);
        }
        catch
        {
            // A diagnostic must never be the reason a process dies.
        }
    }
}
