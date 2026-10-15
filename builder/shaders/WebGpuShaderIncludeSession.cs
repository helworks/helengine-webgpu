using helengine;
using Silk.NET.Shaderc;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;

namespace helengine.webgpu.builder;

/// <summary>Owns rooted shaderc include callbacks and exact active dependency identities for one compilation.</summary>
public sealed unsafe class WebGpuShaderIncludeSession : IDisposable {
    /// <summary>Serializes native allocation ownership and source consistency checks.</summary>
    readonly object Gate = new();
    /// <summary>Borrows the include resolver supplied to the shared backend.</summary>
    readonly IShaderIncludeResolver Resolver;
    /// <summary>Keeps the native resolution delegate alive until shaderc releases its options.</summary>
    readonly IncludeResolveFn ResolveDelegate;
    /// <summary>Keeps the native release delegate alive until all results have retired.</summary>
    readonly IncludeResultReleaseFn ReleaseDelegate;
    /// <summary>Tracks native response blocks that shaderc has not released.</summary>
    readonly HashSet<IntPtr> Allocations = [];
    /// <summary>Records the first content identity returned for each resolved source path.</summary>
    readonly Dictionary<string, WebGpuShaderDependency> Sources = new(StringComparer.Ordinal);
    /// <summary>Provides a preallocated error response when callback allocation itself fails.</summary>
    readonly IntPtr Emergency;
    /// <summary>Retains the first fatal callback exception for propagation after returning from native code.</summary>
    Exception Failure;

    /// <summary>Creates callbacks and an emergency response before the compiler can call into managed code.</summary>
    public WebGpuShaderIncludeSession(IShaderIncludeResolver resolver) {
        Resolver = resolver ?? throw new ArgumentNullException(nameof(resolver));
        Emergency = Allocate("", "WebGPU shader include callback failed.");
        ResolveDelegate = Resolve;
        ReleaseDelegate = Release;
        ResolveCallback = PfnIncludeResolveFn.From(ResolveDelegate);
        ReleaseCallback = PfnIncludeResultReleaseFn.From(ReleaseDelegate);
    }
    /// <summary>Gets the rooted native resolve callback.</summary>
    public PfnIncludeResolveFn ResolveCallback { get; }
    /// <summary>Gets the rooted native release callback.</summary>
    public PfnIncludeResultReleaseFn ReleaseCallback { get; }
    /// <summary>Gets deterministic include identities after native compilation has returned.</summary>
    public IReadOnlyList<WebGpuShaderDependency> Dependencies => Sources.Values.OrderBy(value => value.Path, StringComparer.Ordinal).ToArray();
    /// <summary>Propagates fatal callback failures without throwing through a native stack.</summary>
    public void ThrowIfFailed() {
        if (Failure != null) { throw new InvalidOperationException("WebGPU shader include processing failed: " + Failure.Message, Failure); }
    }
    /// <summary>Resolves an active include with UTF-8 paths and rejects changing content for one identity.</summary>
    IncludeResult* Resolve(void* data, byte* requested, int type, byte* requesting, nuint depth) {
        lock (Gate) {
            try {
                if (depth > 32) { throw new InvalidOperationException("Shader include nesting exceeds 32."); }
                string requestedPath = Marshal.PtrToStringUTF8((IntPtr)requested) ?? throw new InvalidDataException("Missing include name.");
                string requestingPath = Marshal.PtrToStringUTF8((IntPtr)requesting) ?? throw new InvalidDataException("Missing requesting source.");
                ShaderIncludeResult resolved = Resolver.Resolve(requestingPath, requestedPath);
                string hash = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(resolved.Source))).ToLowerInvariant();
                if (Sources.TryGetValue(resolved.Path, out WebGpuShaderDependency previous) && previous.Hash != hash) {
                    throw new InvalidOperationException("Include resolver changed contents during compilation: " + resolved.Path);
                }
                Sources[resolved.Path] = new WebGpuShaderDependency(resolved.Path, hash);
                return (IncludeResult*)Allocate(resolved.Path, resolved.Source);
            } catch (Exception error) {
                if (!(error is FileNotFoundException && type == (int)IncludeType.Relative)) { Failure ??= error; }
                try { return (IncludeResult*)Allocate("", error.Message); }
                catch (Exception allocationError) { Failure ??= allocationError; return (IncludeResult*)Emergency; }
            }
        }
    }
    /// <summary>Allocates header and two NUL-terminated UTF-8 strings as one owned block.</summary>
    IntPtr Allocate(string name, string content) {
        byte[] nameBytes = Encoding.UTF8.GetBytes(name);
        byte[] contentBytes = Encoding.UTF8.GetBytes(content);
        IntPtr block = Marshal.AllocHGlobal(checked(sizeof(IncludeResult) + nameBytes.Length + contentBytes.Length + 2));
        try {
            byte* namePointer = (byte*)block + sizeof(IncludeResult);
            byte* contentPointer = namePointer + nameBytes.Length + 1;
            Marshal.Copy(nameBytes, 0, (IntPtr)namePointer, nameBytes.Length);
            Marshal.Copy(contentBytes, 0, (IntPtr)contentPointer, contentBytes.Length);
            namePointer[nameBytes.Length] = 0; contentPointer[contentBytes.Length] = 0;
            *(IncludeResult*)block = new IncludeResult { SourceName = namePointer, SourceNameLength = (nuint)nameBytes.Length,
                Content = contentPointer, ContentLength = (nuint)contentBytes.Length, UserData = null };
            Allocations.Add(block);
            return block;
        } catch { Marshal.FreeHGlobal(block); throw; }
    }
    /// <summary>Releases one response once, retaining the emergency response until session disposal.</summary>
    void Release(void* data, IncludeResult* result) {
        lock (Gate) {
            if ((IntPtr)result != Emergency && Allocations.Remove((IntPtr)result)) { Marshal.FreeHGlobal((IntPtr)result); }
        }
    }
    /// <summary>Releases native callback handles and any outstanding response allocations after shaderc shutdown.</summary>
    public void Dispose() {
        ResolveCallback.Dispose(); ReleaseCallback.Dispose();
        foreach (IntPtr block in Allocations) { Marshal.FreeHGlobal(block); }
        Allocations.Clear();
    }
}
