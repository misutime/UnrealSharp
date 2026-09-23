using UnrealSharp.Core;
using UnrealSharp.Core.Interop;

namespace UnrealSharp;

/// <summary>
/// Deterministic verification of the development-only (Tier B) checks: a protected entry point that Tier A does
/// not guard has to be refused off the game thread while the switch is on, with the dedicated violation
/// exception, and it must be refused <b>before</b> the native call (the probe's native side would touch manager
/// state, which is exactly what must not happen off the game thread).
///
/// The off-thread probe only runs when the checks are on. With them off there is nothing safe to probe here -
/// that configuration is covered by the configuration matrix, which asserts that Tier A still refuses.
/// </summary>
public static class TierBBoundarySelfTest
{
    public static IReadOnlyList<string> Run()
    {
        List<string> failures = [];

        if (!TierBChecks.Enabled)
        {
            return failures;
        }

        CheckGameThreadDoesNotRefuse(failures);
        CheckThreadPoolIsRefused(failures);

        return failures;
    }

    private static void CheckGameThreadDoesNotRefuse(List<string> failures)
    {
        try
        {
            // A legal game thread call must stay silent: over-refusing is the failure mode that matters more
            // than missing a violation.
            _ = Bind_UCSManager.CallGetCurrentWorldPtr();
        }
        catch (Exception exception)
        {
            failures.Add($"development check refused a legal game thread call ({exception.GetType().Name})");
        }
    }

    private static void CheckThreadPoolIsRefused(List<string> failures)
    {
        int violationsBefore = TierBChecks.ViolationCount;
        string outcome = "not run";

        System.Threading.Tasks.Task.Run(() =>
        {
            try
            {
                _ = Bind_UCSManager.CallGetCurrentWorldPtr();
                outcome = "not refused";
            }
            catch (UnrealSharpThreadViolationException)
            {
                outcome = "refused";
            }
            catch (Exception exception)
            {
                outcome = $"{exception.GetType().Name} instead of {nameof(UnrealSharpThreadViolationException)}";
            }
        }).Wait();

        if (outcome != "refused")
        {
            failures.Add($"the development check on a thread pool thread reported: {outcome}");
            return;
        }

        if (TierBChecks.ViolationCount <= violationsBefore)
        {
            failures.Add("the development check refused the call but did not count the violation");
        }
    }
}
