namespace UnrealSharp.Binds;

/// <summary>
/// Marks a native callback field whose native side already refuses illegal engine access (Tier A).
///
/// The development-only diagnostic (Tier B) must not be added on top of it: the diagnostic would run first and
/// throw before the boundary could refuse, which would turn a refusal into a different failure and would make the
/// always-on counter miss the case. The check still happens on every call - in the native guard, in every
/// configuration including shipping - this attribute only says "do not diagnose this entry twice".
/// </summary>
[AttributeUsage(AttributeTargets.Field)]
public sealed class TierAGuardedAttribute : Attribute;
