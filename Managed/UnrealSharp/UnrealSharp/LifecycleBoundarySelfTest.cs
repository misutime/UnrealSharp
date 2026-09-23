using UnrealSharp.Core;
using UnrealSharp.Core.Interop;

namespace UnrealSharp;

/// <summary>
/// Deterministic verification of the lifecycle half of the guard: releasing, destroying and clearing must either
/// happen completely or not at all, and a refusal inside a finalizer must be recorded instead of escaping (an
/// exception escaping a finalizer terminates the process).
///
/// The positive checks run on the game thread and must stay silent. The finalizer checks are only run behind
/// their own switch, because a regression there kills the process rather than failing a check - which is exactly
/// why they are reserved for a dedicated process.
/// </summary>
public static class LifecycleBoundarySelfTest
{
    private sealed class StrongPointerProbe : TStrongObjectPtr
    {
        public StrongPointerProbe() : base(null)
        {
        }
    }

    /// <summary>Returns one entry per failing check; an empty list means the lifecycle protocol behaved.</summary>
    public static IReadOnlyList<string> Run(bool includeFinalizerChecks)
    {
        List<string> failures = [];

        CheckLegalLifecycleIsSilent(failures);

        if (includeFinalizerChecks)
        {
            CheckFinalizersDoNotEscape(failures);
        }

        return failures;
    }

    private static void CheckLegalLifecycleIsSilent(List<string> failures)
    {
        int refusalsBefore = EngineCallGuard.GetRefusalCount();
        int boundaryCatchesBefore = EngineCallGuard.GetBoundaryCatchCount();

        try
        {
            using FText text = new("lifecycle self test");
            _ = text.ToString();

            using StrongPointerProbe pointer = new();

            if (pointer.IsValid)
            {
                failures.Add("a strong object pointer constructed from null reported itself as valid");
            }
        }
        catch (Exception exception)
        {
            failures.Add($"a legal lifecycle operation was refused ({exception.GetType().Name}: {exception.Message})");
            return;
        }

        int refusals = EngineCallGuard.GetRefusalCount() - refusalsBefore;
        int boundaryCatches = EngineCallGuard.GetBoundaryCatchCount() - boundaryCatchesBefore;

        if (refusals != 0 || boundaryCatches != 0)
        {
            failures.Add(
                $"a legal lifecycle operation recorded a violation (refusals={refusals}, boundaryCatches={boundaryCatches})");
        }
    }

    private static void CheckFinalizersDoNotEscape(List<string> failures)
    {
        // Flush whatever was already waiting for finalization, so the delta below belongs to the probes.
        GC.Collect();
        GC.WaitForPendingFinalizers();

        int boundaryCatchesBefore = EngineCallGuard.GetBoundaryCatchCount();

        CreateAbandonedFinalizerProbes();

        GC.Collect();
        GC.WaitForPendingFinalizers();

        int boundaryCatches = EngineCallGuard.GetBoundaryCatchCount() - boundaryCatchesBefore;

        // Two probes, two refusals recorded at the finalizer boundary; reaching this line at all also proves that
        // no exception escaped a finalizer.
        if (boundaryCatches < 2)
        {
            failures.Add(
                $"the finalizer boundary recorded {boundaryCatches} catch(es), expected at least 2 " +
                "(the process surviving this check is itself part of the evidence)");
        }
    }

    [System.Runtime.CompilerServices.MethodImpl(System.Runtime.CompilerServices.MethodImplOptions.NoInlining)]
    private static void CreateAbandonedFinalizerProbes()
    {
        FText text = new("finalizer boundary self test");
        StrongPointerProbe pointer = new();
        StrongPointerProbe secondPointer = new();

        // Keep them alive until the method returns, so they are collected after it, not during it.
        GC.KeepAlive(text);
        GC.KeepAlive(pointer);
        GC.KeepAlive(secondPointer);
    }
}
