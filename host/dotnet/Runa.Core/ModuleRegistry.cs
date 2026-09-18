namespace McuRemote;

public sealed record ModuleDescriptor(
    ushort ModuleId, byte AbiVersion, byte ModuleVersion, IReadOnlyList<byte> Operations);

public sealed class ModuleRegistry {
    private readonly IReadOnlyDictionary<ushort, ModuleDescriptor> modules;

    public ModuleRegistry(IEnumerable<ModuleDescriptor> descriptors) {
        ArgumentNullException.ThrowIfNull(descriptors);
        Dictionary<ushort, ModuleDescriptor> indexed = new();
        foreach (ModuleDescriptor descriptor in descriptors) {
            if (descriptor.ModuleId == 0 || descriptor.AbiVersion == 0 || descriptor.Operations is null ||
                descriptor.Operations.Count > byte.MaxValue ||
                descriptor.Operations.Distinct().Count() != descriptor.Operations.Count)
                throw new ArgumentException("Invalid module descriptor.", nameof(descriptors));
            if (!indexed.TryAdd(descriptor.ModuleId, descriptor))
                throw new ArgumentException("Duplicate module ID.", nameof(descriptors));
        }
        if (indexed.Count > 16) throw new ArgumentOutOfRangeException(nameof(descriptors));
        modules = indexed;
    }

    public bool Supports(ushort moduleId, byte operation) =>
        modules.TryGetValue(moduleId, out ModuleDescriptor? descriptor) && descriptor.Operations.Contains(operation);

    public IReadOnlyCollection<ModuleDescriptor> Modules => modules.Values.ToArray();
}
