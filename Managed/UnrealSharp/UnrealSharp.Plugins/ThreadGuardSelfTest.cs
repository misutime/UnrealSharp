using UnrealSharp.Core;
using UnrealSharp.Core.Interop;
using UnrealSharp.Interop;
using UnrealSharp.Log;

namespace UnrealSharp.Plugins;

/// <summary>
/// Deterministic verification of the managed half of the engine-call guard.
///
/// The native self test can only exercise the native guards; the managed guard refuses *before* any native call,
/// so nothing on the native side can observe it. This test runs the managed checkpoints for real: once on the
/// game thread, where they must pass, and once on a thread pool thread, where they must refuse with the
/// dedicated exception and report exactly one refusal through the managed channel.
///
/// Every probe passes <c>IntPtr.Zero</c>, so even if a checkpoint were missing the call could not corrupt
/// engine state; what distinguishes the two outcomes is the exception and the refusal count.
/// </summary>
internal static class ThreadGuardSelfTest
{
    private const string LogName = "UnrealSharp";

    /// <summary>
    /// Runs the self test when the startup switch asked for it. Called once, after the native bindings are
    /// available and before the rest of the plugin starts using them.
    /// </summary>
    internal static void RunIfEnabled()
    {
        if (Bind_UCSManager.CallShouldRunManagedThreadSelfTest() == 0)
        {
            return;
        }

        List<string> failures = [];

        RunGameThreadChecks(failures);
        System.Threading.Tasks.Task.Run(() => RunThreadPoolChecks(failures)).Wait();

        // The game thread handoff state machine is verified on an isolated queue, so this does not disturb the
        // queue the plugin is using.
        failures.AddRange(GameThreadWorkQueueSelfTest.Run());

        // The lifecycle protocol; the finalizer half only under its own switch, because a regression there ends
        // the process instead of failing a check.
        failures.AddRange(LifecycleBoundarySelfTest.Run(Bind_UCSManager.CallShouldRunFinalizerThreadSelfTest() != 0));

        // The development-only checks: silent on the game thread, refused off it.
        failures.AddRange(TierBBoundarySelfTest.Run());

        if (failures.Count == 0)
        {
            UnrealLogger.Log(LogName,
                $"Thread guard self test: PASS (managed checkpoints refuse off the game thread and count each refusal once; " +
                $"the game thread handoff settles or refuses every item; lifecycle refusals stay accounted for; " +
                $"development checks silent on the game thread and refusing off it; " +
                $"refusals={EngineCallGuard.GetRefusalCount()}, boundaryCatches={EngineCallGuard.GetBoundaryCatchCount()}, " +
                $"tierBViolations={TierBChecks.ViolationCount})");
            return;
        }

        foreach (string failure in failures)
        {
            UnrealLogger.LogError(LogName, $"Thread guard self test: FAIL {failure}");
        }

        UnrealLogger.LogError(LogName, $"Thread guard self test: FAIL ({failures.Count} failing check(s))");
    }

    private static void RunGameThreadChecks(List<string> failures)
    {
        if (!EngineCallGuard.IsEngineCallSafe)
        {
            failures.Add("the game thread reports an unsafe engine call state");
            return;
        }

        // The guard must not refuse on the game thread: over-refusing would break legal engine access, which is
        // the failure mode that matters more than missing a violation.
        TryExpectNoRefusal(failures, "game thread", nameof(EngineCallGuard.EnsureEngineCallAllowed),
            () => EngineCallGuard.EnsureEngineCallAllowed("self test"));

        TryExpectNoRefusal(failures, "game thread", nameof(Bind_UCSManager.CallFindManagedObject),
            () => Bind_UCSManager.CallFindManagedObject(IntPtr.Zero));

        TryExpectNoRefusal(failures, "game thread", nameof(Bind_UClass.CallGetDefault),
            () => Bind_UClass.CallGetDefault(IntPtr.Zero));
    }

    private static void RunThreadPoolChecks(List<string> failures)
    {
        if (EngineCallGuard.IsEngineCallSafe)
        {
            failures.Add("the thread pool thread reports a safe engine call state");
            return;
        }

        TryExpectRefusal(failures, "thread pool", nameof(EngineCallGuard.EnsureEngineCallAllowed),
            () => EngineCallGuard.EnsureEngineCallAllowed("self test"));

        TryExpectRefusal(failures, "thread pool", nameof(Bind_UCSManager.CallFindManagedObject),
            () => Bind_UCSManager.CallFindManagedObject(IntPtr.Zero));

        TryExpectRefusal(failures, "thread pool", nameof(Bind_UClass.CallGetDefault),
            () => Bind_UClass.CallGetDefault(IntPtr.Zero));
    }

    private static void TryExpectNoRefusal(List<string> failures, string phase, string what, Action probe)
    {
        int before = EngineCallGuard.GetRefusalCount();

        try
        {
            probe();
        }
        catch (Exception exception)
        {
            failures.Add($"{phase}: {what} refused a legal call ({exception.GetType().Name}: {exception.Message})");
            return;
        }

        int refusals = EngineCallGuard.GetRefusalCount() - before;

        if (refusals != 0)
        {
            failures.Add($"{phase}: {what} recorded {refusals} refusal(s) on a legal call");
        }
    }

    private static void TryExpectRefusal(List<string> failures, string phase, string what, Action probe)
    {
        int before = EngineCallGuard.GetRefusalCount();

        try
        {
            probe();
            failures.Add($"{phase}: {what} did not refuse");
            return;
        }
        catch (EngineCallRefusedException)
        {
            // The expected outcome.
        }
        catch (Exception exception)
        {
            failures.Add($"{phase}: {what} refused with {exception.GetType().Name} instead of {nameof(EngineCallRefusedException)}");
            return;
        }

        int refusals = EngineCallGuard.GetRefusalCount() - before;

        if (refusals != 1)
        {
            failures.Add($"{phase}: {what} recorded {refusals} refusals, expected exactly 1");
        }
    }
}
