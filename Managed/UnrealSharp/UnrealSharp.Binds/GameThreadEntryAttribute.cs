namespace UnrealSharp.Binds;

/// <summary>
/// Marks a native callback field whose call touches engine state that is only valid on the game thread.
///
/// The generated wrapper for the field checks the engine state through the managed guard before it invokes
/// the native function, and refuses with a catchable managed exception instead of letting the engine reach a
/// fatal native check. This is the single choke point for the entry points that cannot carry a managed
/// pre-check of their own, such as generated static constructors and generated invokers.
///
/// The check is never disabled by the diagnostics switch: it is a correctness guard, not a diagnostic.
/// Entry points that the guard itself depends on (the state query, the refusal counters, the logging and
/// dispatch paths) must not carry this attribute, or the check would recurse into itself.
/// </summary>
[AttributeUsage(AttributeTargets.Field)]
public sealed class GameThreadEntryAttribute : Attribute
{
    /// <summary>
    /// When true, a null pointer result is a refusal rather than a legal answer, and the wrapper reports it.
    /// Only use this on entry points whose null would otherwise be cached or dereferenced by the caller.
    /// </summary>
    public bool NoNullResult { get; init; }

    /// <summary>
    /// When true, a negative integer result is a refusal rather than a legal answer. Only use this where the
    /// native side answers a refusal with a negative sentinel and no legitimate answer is negative.
    /// </summary>
    public bool NonNegativeResult { get; init; }
}
