using System.Collections.Concurrent;
using System.Runtime.CompilerServices;
using UnrealSharp.Core.Interop;

namespace UnrealSharp.Core;

/// <summary>
/// Raised by a development-only (Tier B) check when engine state is touched from a thread or in a state where the
/// engine does not allow it.
///
/// It derives from <see cref="EngineCallRefusedException"/> on purpose: callers that already handle a refusal
/// keep working, and boundaries where an exception must not escape can catch the whole family with one clause.
/// The difference is who noticed: a refusal comes from the always-on boundary (Tier A), a violation comes from a
/// development build diagnostic that a shipping build does not contain.
/// </summary>
public sealed class UnrealSharpThreadViolationException : EngineCallRefusedException
{
    public UnrealSharpThreadViolationException(string message) : base(message)
    {
    }
}

/// <summary>
/// The development-only (Tier B) checks.
///
/// Tier A refuses engine access that the boundary itself protects (creating objects, registering types,
/// resolving fields, lifecycle). Tier B covers what the boundary cannot afford to check on every call: property
/// reads and writes, marshalling and container views. It exists to make a violation *loud* in a development
/// build, not to be the last line of defence, so it is allowed to be more expensive and it is switched off in
/// shipping builds.
///
/// Deliberately not enabled per call site: the switch is a startup snapshot read once into a
/// <c>static readonly</c> field, so a legal call pays one field read plus a native state query.
/// </summary>
public static class TierBChecks
{
    /// <summary>Startup snapshot of the development switch. Never consulted by Tier A.</summary>
    public static readonly bool Enabled = IsEnabledNatively();

    /// <summary>
    /// The managed assemblies can be rebuilt on their own, before the native plugin is. In that window the
    /// binding is missing, and answering "off" keeps the development checks from calling through a function
    /// pointer this native binary does not provide - which would take the process down during static
    /// initialisation instead of merely skipping a diagnostic.
    /// </summary>
    private static unsafe bool IsEnabledNatively()
    {
        if (Bind_UCSManager.IsTierBEnabled == null)
        {
            return false;
        }

        return Bind_UCSManager.CallIsTierBEnabled() != 0;
    }

    /// <summary>Number of Tier B violations seen so far, regardless of how many were logged.</summary>
    public static int ViolationCount => _violationCount;

    private static readonly ConcurrentDictionary<string, byte> LoggedCategories = new();
    private static int _violationCount;

    /// <summary>
    /// Checks one protected access. Answers immediately when the checks are off or engine state is safe, so this
    /// is the only thing a legal call executes.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void Check(string category, string what)
    {
        if (!Enabled || EngineCallGuard.IsEngineCallSafe)
        {
            return;
        }

        Report(category, what);
    }

    /// <summary>
    /// Same as <see cref="Check"/>, but for the first access of a generated accessor where the managed side
    /// already knows the address it is about to read. Kept separate so the message can name the field.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void CheckAccess(string category, string what)
    {
        Check(category, what);
    }

    private static void Report(string category, string what)
    {
        System.Threading.Interlocked.Increment(ref _violationCount);

        // Refusals are counted natively as well, so the operator sees them in the same place as a Tier A refusal.
        try
        {
            Bind_UCSManager.CallRecordManagedRefusal(what);
        }
        catch
        {
            // A diagnostic must never be the reason a call site dies.
        }

        // One full message per category, and only the count afterwards: a violation inside a loop would
        // otherwise flood the log with the same sentence.
        if (!LoggedCategories.TryAdd(category, 0))
        {
            return;
        }

        int state = EngineCallGuard.GetEngineCallState();
        bool onGameThread = (state & 1) != 0;
        bool collectingGarbage = (state & 2) != 0;
        bool lockingHashTables = (state & 4) != 0;

        string message =
            $"{what} touched engine state illegally (category '{category}'): " +
            $"onGameThread={onGameThread}, collectingGarbage={collectingGarbage}, lockingHashTables={lockingHashTables}, " +
            $"managedThread={System.Environment.CurrentManagedThreadId}. " +
            "This is a development-only check: route the work through GameThreadDispatcher.RunAsync (and keep it " +
            "inside the synchronous delegate) or move it to code that already runs on the game thread. " +
            "Further violations in this category are counted without being logged again.";

        try
        {
            LogUnrealSharpCore.LogError($"UnrealSharp thread violation: {message}");
        }
        catch
        {
        }

        throw new UnrealSharpThreadViolationException(message);
    }
}
