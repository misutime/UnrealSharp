using System.Collections.Concurrent;
using System.Reflection;
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
///
/// The handoff is a state machine with explicit judgement points, so a task can never be left pending:
///
/// | state                        | judgement point              | task result            | handle owner |
/// |------------------------------|------------------------------|------------------------|--------------|
/// | refused before submission    | managed, before any handle   | faulted                | nobody       |
/// | not accepted by the native   | the native entry point       | faulted                | managed      |
/// | accepted, waiting to run     | native queued / ran inline   | completes or faults    | native       |
/// | ran successfully             | on the game thread           | success                | native       |
/// | refused while running        | re-checked on the game thread| faulted, action not run| native       |
///
/// The handle handed to native belongs to this queue, not to the caller's delegate: a queued item therefore
/// never keeps a collectible assembly alive, and after <see cref="DiscardPendingForAssembly"/> the item is
/// simply not found any more instead of invoking a delegate from an unloaded assembly.
/// </summary>
public sealed class GameThreadWorkQueue
{
    private sealed class PendingWork
    {
        public required Action Action;
        public required TaskCompletionSource Completion;
        public required Assembly? SubmittingAssembly;
    }

    private readonly ConcurrentDictionary<long, PendingWork> _pending = new();
    private long _nextWorkId;
    private volatile bool _acceptingWork = true;

    private int _refusedBeforeSubmission;
    private int _refusedByNative;
    private int _handedToNative;
    private int _refusedWhileRunning;

    /// <summary>Work items that are currently accepted and not yet executed. Diagnostics.</summary>
    public int PendingCount => _pending.Count;

    /// <summary>Submissions refused on the managed side, before a handle was allocated. Diagnostics.</summary>
    public int RefusedBeforeSubmissionCount => _refusedBeforeSubmission;

    /// <summary>Submissions the native dispatcher refused, where the managed side released the handle. Diagnostics.</summary>
    public int RefusedByNativeCount => _refusedByNative;

    /// <summary>Submissions the game thread took ownership of. Diagnostics.</summary>
    public int HandedToNativeCount => _handedToNative;

    /// <summary>Items that were accepted but refused when their turn came, so the action never ran. Diagnostics.</summary>
    public int RefusedWhileRunningCount => _refusedWhileRunning;

    /// <summary>
    /// Stops accepting work and settles everything still pending. Called on an orderly shutdown, before the
    /// assemblies that submitted the work are unloaded: after this, no queued action runs and no awaiting
    /// caller is left waiting.
    /// </summary>
    public void Shutdown()
    {
        _acceptingWork = false;
        SettleAllPending(new EngineCallRefusedException(
            "The game thread dispatcher was shut down; the queued work was not executed."));
    }

    /// <summary>
    /// Drops and settles the pending work submitted from one assembly. Called before that assembly is unloaded,
    /// so the queue does not keep a delegate of an unloaded assembly alive and cannot invoke one afterwards.
    /// </summary>
    public int DiscardPendingForAssembly(Assembly assembly)
    {
        int discarded = 0;

        foreach (KeyValuePair<long, PendingWork> entry in _pending)
        {
            if (entry.Value.SubmittingAssembly != assembly)
            {
                continue;
            }

            if (_pending.TryRemove(entry.Key, out PendingWork? work))
            {
                work.Completion.TrySetException(new InvalidOperationException(
                    $"The assembly '{assembly.GetName().Name}' was unloaded; the queued game thread work was discarded."));
                discarded++;
            }
        }

        return discarded;
    }

    public Task RunAsync(Action action)
    {
        ArgumentNullException.ThrowIfNull(action);

        TaskCompletionSource completion = new(TaskCreationOptions.RunContinuationsAsynchronously);

        // Judgement point 1: refused before a handle exists, so there is nothing to hand over and nothing to
        // release. A caller that is told "no" here can react; a caller left pending forever cannot.
        if (!_acceptingWork)
        {
            Interlocked.Increment(ref _refusedBeforeSubmission);
            completion.TrySetException(new EngineCallRefusedException(
                "The game thread dispatcher is shut down and no longer accepts work."));
            return completion.Task;
        }

        long workId = Interlocked.Increment(ref _nextWorkId);
        _pending[workId] = new PendingWork
        {
            Action = action,
            Completion = completion,
            SubmittingAssembly = action.Method.DeclaringType?.Assembly ?? action.Target?.GetType().Assembly
        };

        GCHandle callbackHandle = GCHandle.Alloc(new Action(() => TryExecuteWorkItem(workId)));
        IntPtr handlePointer = GCHandle.ToIntPtr(callbackHandle);

        int accepted;
        try
        {
            accepted = Bind_Async.CallTryRunOnGameThread(handlePointer);
        }
        catch
        {
            // The native call itself failed: nothing was handed over, so this side still owns the handle.
            _pending.TryRemove(workId, out _);
            callbackHandle.Free();
            throw;
        }

        // Judgement point 2: the native entry point decides whether the game thread will ever run this. If it
        // says no, this side is still the only owner of the handle and fails the task right away.
        if (accepted == 0)
        {
            _pending.TryRemove(workId, out _);
            callbackHandle.Free();
            Interlocked.Increment(ref _refusedByNative);
            completion.TrySetException(new EngineCallRefusedException(
                "The native game thread dispatcher refused the work (the engine is exiting); the task was not queued."));
            return completion.Task;
        }

        Interlocked.Increment(ref _handedToNative);
        return completion.Task;
    }

    /// <summary>
    /// Test seam: registers work without handing it to the engine, so the execution judgement can be driven
    /// directly instead of depending on when the game thread happens to tick. Used by the self test only.
    /// </summary>
    internal long QueueWithoutDispatch(Action action, Assembly? submittingAssembly = null)
    {
        long workId = Interlocked.Increment(ref _nextWorkId);
        _pending[workId] = new PendingWork
        {
            Action = action,
            Completion = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously),
            SubmittingAssembly = submittingAssembly ?? action.Method.DeclaringType?.Assembly
        };
        return workId;
    }

    /// <summary>Diagnostics entry for the self test: the completion of an item registered with the seam above.</summary>
    internal Task GetQueuedTask(long workId)
    {
        return _pending.TryGetValue(workId, out PendingWork? work) ? work.Completion.Task : Task.CompletedTask;
    }

    /// <summary>
    /// Runs one queued work item. This is what the game thread calls through the native dispatcher, and it is
    /// judgement point 3: when the queue was shut down or the item was discarded, or when engine state is not
    /// safe at this moment, the caller's action is <b>not</b> started and the task is failed instead.
    /// </summary>
    internal bool TryExecuteWorkItem(long workId)
    {
        if (!_pending.TryRemove(workId, out PendingWork? work))
        {
            // Shut down or discarded while queued: the action must not run.
            return false;
        }

        if (!_acceptingWork)
        {
            Interlocked.Increment(ref _refusedWhileRunning);
            work.Completion.TrySetException(new EngineCallRefusedException(
                "The game thread dispatcher was shut down before the queued work could run."));
            return false;
        }

        if (!EngineCallGuard.IsEngineCallSafe)
        {
            Interlocked.Increment(ref _refusedWhileRunning);
            work.Completion.TrySetException(new EngineCallRefusedException(
                "Engine state is not safe for the queued game thread work (a garbage collection is running or " +
                "this is not the game thread); the action was not started."));
            return false;
        }

        try
        {
            work.Action();
            work.Completion.TrySetResult();
        }
        catch (Exception exception)
        {
            // Nothing may escape into the native caller: an escaping exception would take the process down
            // instead of failing the task.
            work.Completion.TrySetException(exception);
        }

        return true;
    }

    private void SettleAllPending(Exception exception)
    {
        foreach (KeyValuePair<long, PendingWork> entry in _pending)
        {
            if (_pending.TryRemove(entry.Key, out PendingWork? work))
            {
                work.Completion.TrySetException(exception);
            }
        }
    }
}

/// <summary>
/// Process wide entry point of the game thread dispatcher. The state machine itself lives in
/// <see cref="GameThreadWorkQueue"/>; this type owns the queue the plugin uses.
/// </summary>
public static class GameThreadDispatcher
{
    private static readonly GameThreadWorkQueue DefaultQueue = new();

    /// <summary>
    /// True when engine state may be touched right now: on the game thread and outside garbage collection.
    /// </summary>
    public static bool IsEngineCallSafe => EngineCallGuard.IsEngineCallSafe;

    /// <summary>
    /// Fails with a descriptive managed exception when the calling thread must not touch engine state.
    /// </summary>
    public static void EnsureEngineCallAllowed(string what) => EngineCallGuard.EnsureEngineCallAllowed(what);

    public static Task RunAsync(Action action) => DefaultQueue.RunAsync(action);

    /// <summary>
    /// Orders the dispatcher to stop accepting work and to settle what is still pending. Call this on an
    /// orderly shutdown, before the assemblies that submitted work are unloaded.
    /// </summary>
    public static void Shutdown() => DefaultQueue.Shutdown();

    /// <summary>
    /// Discards the pending work of one assembly. Called before that assembly is unloaded.
    /// </summary>
    public static int DiscardPendingForAssembly(Assembly assembly) => DefaultQueue.DiscardPendingForAssembly(assembly);

    /// <summary>Diagnostics counters of the shared queue.</summary>
    public static GameThreadWorkQueue Default => DefaultQueue;

    /// <summary>
    /// A separate queue, used by the deterministic self test so it can exercise the state machine without
    /// disturbing the queue the plugin is using.
    /// </summary>
    public static GameThreadWorkQueue CreateIsolatedQueue() => new();
}
