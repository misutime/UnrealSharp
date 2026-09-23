using System.Collections.Generic;
using System.Linq;
using System.Text;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.Text;
using UnrealSharp.SourceGenerator.Utilities;

namespace UnrealSharp.SourceGenerators;

public struct ParameterInfo
{
    public DelegateParameterInfo Parameter { get; set; }
}

[Generator]
public class NativeCallbacksWrapperGenerator : IIncrementalGenerator
{
    public void Initialize(IncrementalGeneratorInitializationContext context)
    {
        var classDeclarations = context.SyntaxProvider.CreateSyntaxProvider(
                static (syntaxNode, _) => syntaxNode is ClassDeclarationSyntax cds && cds.AttributeLists.Count > 0,
                static (syntaxContext, _) => GetClassInfoOrNull(syntaxContext));

        var classAndCompilation = classDeclarations.Combine(context.CompilationProvider);

        context.RegisterSourceOutput(classAndCompilation, (spc, pair) =>
        {
            var maybeClassInfo = pair.Left; // ClassInfo?
            var compilation = pair.Right;
            if (!maybeClassInfo.HasValue)
            {
                return;
            }
            GenerateForClass(spc, compilation, maybeClassInfo.Value);
        });
    }

    private static void GenerateForClass(SourceProductionContext context, Compilation compilation, ClassInfo classInfo)
    {
        var model = compilation.GetSemanticModel(classInfo.ClassDeclaration.SyntaxTree);
        var sourceBuilder = new StringBuilder();

        HashSet<string> namespaces = [];
        foreach (DelegateInfo delegateInfo in classInfo.Delegates)
        {
            foreach (var parameter in delegateInfo.ParametersAndReturnValue)
            {
                var typeInfo = model.GetTypeInfo(parameter.Type);
                var typeSymbol = typeInfo.Type;

                if (typeSymbol == null || typeSymbol.ContainingNamespace == null)
                {
                    continue;
                }

                if (typeSymbol is INamedTypeSymbol nts && nts.IsGenericType)
                {
                    namespaces.UnionWith(nts.TypeArguments.Where(t => t.ContainingNamespace != null).Select(t => t.ContainingNamespace!.ToDisplayString()));
                }

                namespaces.Add(typeSymbol.ContainingNamespace.ToDisplayString());
            }
        }

        if (classInfo.NullableAwareable)
        {
            sourceBuilder.AppendLine("#nullable enable");
        }
        else
        {
            sourceBuilder.AppendLine("#nullable disable");
        }
        
        sourceBuilder.AppendLine();

        foreach (string? ns in namespaces)
        {
            if (string.IsNullOrWhiteSpace(ns)) continue;
            sourceBuilder.AppendLine($"using {ns};");
        }

        sourceBuilder.AppendLine();
        sourceBuilder.AppendLine($"namespace {classInfo.Namespace}");
        sourceBuilder.AppendLine("{");
        sourceBuilder.AppendLine($"    public static unsafe partial class {classInfo.Name}");
        sourceBuilder.AppendLine("    {");

        sourceBuilder.AppendLine("        static " + classInfo.Name + "()");
        sourceBuilder.AppendLine("        {");

        foreach (DelegateInfo delegateInfo in classInfo.Delegates)
        {
            string delegateName = delegateInfo.Name;

            string totalSizeDelegateName = delegateName + "TotalSize";
            if (!delegateInfo.HasReturnValue && delegateInfo.Parameters.Count == 0)
            {
                sourceBuilder.AppendLine($"             int {totalSizeDelegateName} = 0;");
            }
            else
            {
                sourceBuilder.Append($"             int {totalSizeDelegateName} = ");

                void AppendSizeOf(DelegateParameterInfo param)
                {
                    string typeFullName = param.Type.GetAnnotatedTypeName(model) ?? param.Type.ToString();

                    if (param.IsOutParameter || param.IsRefParameter)
                    {
                        sourceBuilder.Append($"IntPtr.Size");
                    }
                    else
                    {
                        sourceBuilder.Append($"sizeof({typeFullName})");
                    }
                }

                List<DelegateParameterInfo> parameters = delegateInfo.ParametersAndReturnValue;

                for (int i = 0; i < parameters.Count; i++)
                {
                    AppendSizeOf(parameters[i]);

                    if (i != parameters.Count - 1)
                    {
                        sourceBuilder.Append(" + ");
                    }
                }

                sourceBuilder.AppendLine(";");
            }

            string funcPtrName = delegateName + "FuncPtr";
            sourceBuilder.AppendLine($"             IntPtr {funcPtrName} = UnrealSharp.Binds.NativeBinds.TryGetBoundFunction(\"{classInfo.Name}\", \"{delegateInfo.Name}\", {totalSizeDelegateName});");
            sourceBuilder.Append($"             {delegateName} = (delegate* unmanaged<");
            sourceBuilder.Append(string.Join(", ", delegateInfo.Parameters.Select(p =>
            {
                string prefix = p.IsOutParameter ? "out " : p.IsRefParameter ? "ref " : string.Empty;
                return prefix + (p.Type.GetAnnotatedTypeName(model) ?? p.Type.ToString());
            })));

            if (delegateInfo.Parameters.Count > 0)
            {
                sourceBuilder.Append(", ");
            }
            
            sourceBuilder.Append(delegateInfo.ReturnValue.Type.GetAnnotatedTypeName(model) ?? delegateInfo.ReturnValue.Type.ToString());

            sourceBuilder.Append($">){funcPtrName};");
            sourceBuilder.AppendLine();
        }

        sourceBuilder.AppendLine("        }");

        foreach (DelegateInfo delegateInfo in classInfo.Delegates)
        {
            string returnTypeFullName = delegateInfo.ReturnValue.Type.GetAnnotatedTypeName(model) ?? delegateInfo.ReturnValue.Type.ToString();
            sourceBuilder.AppendLine($"        [System.Runtime.CompilerServices.MethodImpl(System.Runtime.CompilerServices.MethodImplOptions.AggressiveInlining)]");
            sourceBuilder.Append($"        public static {returnTypeFullName} Call{delegateInfo.Name}(");

            bool firstParameter = true;
            foreach (DelegateParameterInfo parameter in delegateInfo.Parameters)
            {
                if (!firstParameter)
                {
                    sourceBuilder.Append(", ");
                }

                firstParameter = false;

                if (parameter.IsOutParameter)
                {
                    sourceBuilder.Append("out ");
                }

                if (parameter.IsRefParameter)
                {
                    sourceBuilder.Append("ref ");
                }

                string typeFullName = parameter.Type.GetAnnotatedTypeName(model) ?? parameter.Type.ToString();
                sourceBuilder.Append($"{typeFullName} {parameter.Name}");
            }

            sourceBuilder.AppendLine(")");
            sourceBuilder.AppendLine("        {");

            string delegateName = delegateInfo.Name;

            // Entry points marked as game thread work are refused by the wrapper itself. This is the single
            // choke point that also covers generated static constructors and generated invokers, which have no
            // hand written call site to guard.
            if (delegateInfo.RequiresGameThread)
            {
                sourceBuilder.AppendLine($"            global::UnrealSharp.Core.EngineCallGuard.EnsureEngineCallAllowed(\"{delegateName}\");");
            }
            else if (!delegateInfo.TierAGuarded && !IsTierBExempt(classInfo.Name, delegateName))
            {
                // Development-only diagnostic (Tier B). It is skipped for entry points whose native side already
                // refuses (Tier A) - the diagnostic would fire first and turn a refusal into a different failure -
                // and for the symbols the diagnostics themselves depend on.
                sourceBuilder.AppendLine($"            global::UnrealSharp.Core.TierBChecks.Check(\"{classInfo.Name}\", \"{delegateName}\");");
            }

            bool hasReturnValue = delegateInfo.ReturnValue.Type.ToString() != "void";
            string returnTypeName = delegateInfo.ReturnValue.Type.ToString();
            bool validateNullResult = hasReturnValue && delegateInfo.NoNullResult && IsPointerTypeName(returnTypeName);
            bool validateNegativeResult = hasReturnValue && delegateInfo.NonNegativeResult && IsIntegerTypeName(returnTypeName);
            bool captureResult = validateNullResult || validateNegativeResult;

            string invocation = $"{delegateName}(" + string.Join(", ", delegateInfo.Parameters.Select(p =>
            {
                string prefix = p.IsOutParameter ? "out " : p.IsRefParameter ? "ref " : string.Empty;
                return prefix + p.Name;
            })) + ")";

            if (captureResult)
            {
                sourceBuilder.AppendLine($"            {returnTypeName} result = {invocation};");

                if (validateNullResult)
                {
                    sourceBuilder.AppendLine($"            global::UnrealSharp.Core.EngineCallGuard.EnsureNotNullResult(result, \"{delegateName}\");");
                }
                else
                {
                    sourceBuilder.AppendLine($"            global::UnrealSharp.Core.EngineCallGuard.EnsureNotNegativeResult(result, \"{delegateName}\");");
                }

                sourceBuilder.AppendLine("            return result;");
            }
            else if (hasReturnValue)
            {
                sourceBuilder.AppendLine($"            return {invocation};");
            }
            else
            {
                sourceBuilder.AppendLine($"            {invocation};");
            }

            sourceBuilder.AppendLine("        }");
        }

        // End class definition
        sourceBuilder.AppendLine("    }");
        sourceBuilder.AppendLine("}");

        context.AddSource($"{classInfo.Name}.generated.cs", SourceText.From(sourceBuilder.ToString(), Encoding.UTF8));
    }

    private static void ReadGameThreadEntryAttribute(GeneratorSyntaxContext context, FieldDeclarationSyntax fieldDeclaration, ref DelegateInfo delegateInfo)
    {
        if (context.SemanticModel.GetDeclaredSymbol(fieldDeclaration.Declaration.Variables.First()) is not IFieldSymbol fieldSymbol)
        {
            return;
        }

        foreach (AttributeData attribute in fieldSymbol.GetAttributes())
        {
            string? attributeName = attribute.AttributeClass?.Name;

            if (attributeName == "TierAGuardedAttribute")
            {
                delegateInfo.TierAGuarded = true;
                continue;
            }

            if (attributeName != "GameThreadEntryAttribute")
            {
                continue;
            }

            delegateInfo.RequiresGameThread = true;

            foreach (KeyValuePair<string, TypedConstant> namedArgument in attribute.NamedArguments)
            {
                if (namedArgument.Value.Value is not bool value)
                {
                    continue;
                }

                if (namedArgument.Key == "NoNullResult")
                {
                    delegateInfo.NoNullResult = value;
                }
                else if (namedArgument.Key == "NonNegativeResult")
                {
                    delegateInfo.NonNegativeResult = value;
                }
            }
        }
    }

    /// <summary>
    /// Entry points the development-only diagnostic must not wrap, because the diagnostic itself goes through
    /// them: the state query, the refusal counters, the logging and dispatch paths, and the pure thread queries
    /// that are meaningful on any thread. Every entry here is a deliberate per symbol decision; the always-on
    /// boundary does not depend on this list.
    /// </summary>
    private static bool IsTierBExempt(string className, string functionName)
    {
        return (className, functionName) switch
        {
            ("Bind_UCSManager", "GetEngineCallState") => true,
            ("Bind_UCSManager", "GetThreadRefusalCount") => true,
            ("Bind_UCSManager", "GetThreadBoundaryCatchCount") => true,
            ("Bind_UCSManager", "ReportBoundaryCatch") => true,
            ("Bind_UCSManager", "RecordManagedRefusal") => true,
            ("Bind_UCSManager", "ShouldRunManagedThreadSelfTest") => true,
            ("Bind_UCSManager", "ShouldRunFinalizerThreadSelfTest") => true,
            ("Bind_UCSManager", "IsTierBEnabled") => true,
            ("Bind_UCSManager", "IsOnGameThread") => true,
            ("Bind_UCSManager", "IsCollectingGarbage") => true,
            ("Bind_FMsg", "Log") => true,
            ("Bind_Async", "RunOnThread") => true,
            ("Bind_Async", "RunOnGameThread") => true,
            ("Bind_Async", "TryRunOnGameThread") => true,
            ("Bind_Async", "GetCurrentNamedThread") => true,
            _ => false
        };
    }

    private static bool IsPointerTypeName(string typeName)
    {
        return typeName is "IntPtr" or "nint" or "System.IntPtr" or "System.nint";
    }

    private static bool IsIntegerTypeName(string typeName)
    {
        return typeName is "int" or "System.Int32";
    }

    private static ClassInfo? GetClassInfoOrNull(GeneratorSyntaxContext context)
    {
        if (context.Node is not ClassDeclarationSyntax classDeclaration)
        {
            return null;
        }

        // Check attribute (support both NativeCallbacks and NativeCallbacksAttribute)
        bool hasNativeCallbacksAttribute = classDeclaration.AttributeLists
            .SelectMany(a => a.Attributes)
            .Any(a => a.Name.ToString() is "NativeCallbacks" or "NativeCallbacksAttribute");

        if (!hasNativeCallbacksAttribute)
        {
            return null;
        }

        string namespaceName = classDeclaration.GetFullNamespace();

        if (string.IsNullOrEmpty(namespaceName))
        {
            return null;
        }

        var classInfo = new ClassInfo
        {
            ClassDeclaration = classDeclaration,
            Name = classDeclaration.Identifier.ValueText,
            Namespace = namespaceName,
            Delegates = new List<DelegateInfo>(),
            NullableAwareable = context.SemanticModel.GetNullableContext(context.Node.Span.Start).HasFlag(NullableContext.AnnotationsEnabled)
        };

        foreach (MemberDeclarationSyntax member in classDeclaration.Members)
        {
            if (member is not FieldDeclarationSyntax fieldDeclaration ||
                fieldDeclaration.Declaration.Type is not FunctionPointerTypeSyntax functionPointerTypeSyntax)
            {
                continue;
            }

            var delegateInfo = new DelegateInfo
            {
                Name = fieldDeclaration.Declaration.Variables.First().Identifier.ValueText,
                Parameters = new List<DelegateParameterInfo>()
            };

            ReadGameThreadEntryAttribute(context, fieldDeclaration, ref delegateInfo);

            char paramName = 'a';

            for (int i = 0; i < functionPointerTypeSyntax.ParameterList.Parameters.Count; i++)
            {
                FunctionPointerParameterSyntax param = functionPointerTypeSyntax.ParameterList.Parameters[i];

                DelegateParameterInfo parameter = new DelegateParameterInfo
                {
                    Name = paramName.ToString(),
                    IsOutParameter = param.Modifiers.Any(modifier => modifier.IsKind(SyntaxKind.OutKeyword)),
                    IsRefParameter = param.Modifiers.Any(modifier => modifier.IsKind(SyntaxKind.RefKeyword)),
                    Type = param.Type,
                };

                bool isReturnParameter = i == functionPointerTypeSyntax.ParameterList.Parameters.Count - 1;
                if (isReturnParameter)
                {
                    delegateInfo.ReturnValue = parameter;
                }
                else
                {
                    delegateInfo.Parameters.Add(parameter);
                }

                paramName++;
            }

            classInfo.Delegates.Add(delegateInfo);
        }

        return classInfo;
    }
}

internal struct ClassInfo
{
    public ClassDeclarationSyntax ClassDeclaration;
    public string Name;
    public string Namespace;
    public List<DelegateInfo> Delegates;
    public bool NullableAwareable;
}

internal struct DelegateInfo
{
    public string Name;
    public List<DelegateParameterInfo> Parameters;

    /// <summary>Set when the field carries the game thread entry attribute and the wrapper must refuse off-thread calls.</summary>
    public bool RequiresGameThread;

    /// <summary>Set when the native side of the entry point already refuses illegal engine access (Tier A).</summary>
    public bool TierAGuarded;

    /// <summary>Set when a null pointer result must be reported as a refusal instead of handed to the caller.</summary>
    public bool NoNullResult;

    /// <summary>Set when a negative integer result must be reported as a refusal instead of handed to the caller.</summary>
    public bool NonNegativeResult;
    public List<DelegateParameterInfo> ParametersAndReturnValue
    {
        get
        {
            List<DelegateParameterInfo> allParameters = new List<DelegateParameterInfo>(Parameters);

            if (ReturnValue.Type.ToString() != "void")
            {
                allParameters.Add(ReturnValue);
            }

            return allParameters;
        }
    }

    public bool HasReturnValue => ReturnValue.Type.ToString() != "void";
    public DelegateParameterInfo ReturnValue;
}

public struct DelegateParameterInfo
{
    public string Name;
    public TypeSyntax Type;
    public bool IsOutParameter;
    public bool IsRefParameter;
}