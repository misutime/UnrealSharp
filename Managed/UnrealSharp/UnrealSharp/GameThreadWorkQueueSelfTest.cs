using System.Reflection;

namespace UnrealSharp;

/// <summary>
/// Deterministic verification of the game thread handoff state machine.
///
/// Every judgement point is driven directly on an isolated queue, so the test does not depend on when the game
/// thread happens to tick and never has to leave work pending in the queue the plugin is using. What it asserts
/// is the property the state machine exists for: work is either handed over and completed, or refused with a
/// failed task, and in no case is it silently dropped.
/// </summary>
public static class GameThreadWorkQueueSelfTest
{
    /// <summary>Returns one entry per failing check; an empty list means the state machine behaved.</summary>
    public static IReadOnlyList<string> Run()
    {
        List<string> failures = [];

        CheckRefusedBeforeSubmission(failures);
        CheckRefusedAfterShutdown(failures);
        CheckDiscardedWorkDoesNotRun(failures);
        CheckAcceptedWorkCompletes(failures);

        return failures;
    }

    private static void CheckRefusedBeforeSubmission(List<string> failures)
    {
        GameThreadWorkQueue queue = GameThreadDispatcher.CreateIsolatedQueue();
        queue.Shutdown();

        bool ran = false;
        Task task = queue.RunAsync(() => ran = true);

        if (ran)
        {
            failures.Add("a shut down queue still ran the submitted action");
        }

        if (!task.IsFaulted)
        {
            failures.Add("a shut down queue did not fail the submitted task");
        }

        if (queue.HandedToNativeCount != 0 || queue.RefusedBeforeSubmissionCount != 1)
        {
            failures.Add(
                $"refusal before submission was not counted as expected (handedToNative={queue.HandedToNativeCount}, " +
                $"refusedBeforeSubmission={queue.RefusedBeforeSubmissionCount})");
        }
    }

    private static void CheckRefusedAfterShutdown(List<string> failures)
    {
        GameThreadWorkQueue queue = GameThreadDispatcher.CreateIsolatedQueue();

        bool ran = false;
        long workId = queue.QueueWithoutDispatch(() => ran = true);
        Task queued = queue.GetQueuedTask(workId);

        queue.Shutdown();

        if (!queued.IsFaulted)
        {
            failures.Add("shutdown did not settle the queued task");
        }

        if (queue.TryExecuteWorkItem(workId))
        {
            failures.Add("a work item that was settled by shutdown still reported that it ran");
        }

        if (ran)
        {
            failures.Add("a work item that was settled by shutdown still ran its action");
        }

        if (queue.PendingCount != 0)
        {
            failures.Add($"shutdown left {queue.PendingCount} work item(s) pending");
        }
    }

    private static void CheckDiscardedWorkDoesNotRun(List<string> failures)
    {
        GameThreadWorkQueue queue = GameThreadDispatcher.CreateIsolatedQueue();
        Assembly submittingAssembly = typeof(GameThreadWorkQueueSelfTest).Assembly;

        bool ran = false;
        long workId = queue.QueueWithoutDispatch(() => ran = true, submittingAssembly);
        Task queued = queue.GetQueuedTask(workId);

        int discarded = queue.DiscardPendingForAssembly(submittingAssembly);

        if (discarded != 1)
        {
            failures.Add($"discarding the pending work of an assembly reported {discarded} item(s), expected 1");
        }

        if (!queued.IsFaulted)
        {
            failures.Add("discarding the pending work did not settle the queued task");
        }

        if (queue.TryExecuteWorkItem(workId))
        {
            failures.Add("a discarded work item still reported that it ran");
        }

        if (ran)
        {
            failures.Add("a discarded work item still ran its action");
        }

        if (queue.PendingCount != 0)
        {
            failures.Add($"discarding left {queue.PendingCount} work item(s) pending");
        }
    }

    private static void CheckAcceptedWorkCompletes(List<string> failures)
    {
        GameThreadWorkQueue queue = GameThreadDispatcher.CreateIsolatedQueue();

        bool ran = false;
        Task task = queue.RunAsync(() => ran = true);

        // The self test runs on the game thread during startup, so the native dispatcher runs the work inline
        // and the task is already settled here.
        if (!task.IsCompletedSuccessfully)
        {
            failures.Add($"accepted work did not complete (status={task.Status})");
        }

        if (!ran)
        {
            failures.Add("accepted work did not run its action");
        }

        if (queue.HandedToNativeCount != 1)
        {
            failures.Add($"accepted work was not handed to the native dispatcher (handedToNative={queue.HandedToNativeCount})");
        }

        if (queue.PendingCount != 0)
        {
            failures.Add($"accepted work left {queue.PendingCount} work item(s) pending");
        }

        if (queue.RefusedWhileRunningCount != 0 || queue.RefusedByNativeCount != 0)
        {
            failures.Add(
                $"accepted work recorded a refusal (whileRunning={queue.RefusedWhileRunningCount}, byNative={queue.RefusedByNativeCount})");
        }
    }
}
