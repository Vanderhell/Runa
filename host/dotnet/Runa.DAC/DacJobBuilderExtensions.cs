using System.Buffers.Binary;
namespace McuRemote;
public sealed record DacResourceConfiguration(uint MaximumValue, byte ResolutionBits);
public static class DacJobBuilderExtensions {
    public const ushort ModuleId = 9; public const ushort ResourceType = 1; public const byte WriteOperation = 1;
    public static ModuleDescriptor Descriptor { get; } = new(ModuleId, 1, 1, [WriteOperation]);
    public static void DacWrite(this JobBuilder builder, ResourceDescriptor resource, Register value) {
        ArgumentNullException.ThrowIfNull(builder); ValidateResource(resource);
        Span<byte> payload = stackalloc byte[3]; BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id); payload[2] = value.Index;
        builder.EmitExtension(ModuleId, WriteOperation, payload);
    }
    public static void DacWrite(this JobBuilder builder, ResourceDescriptor resource, uint value) {
        DacResourceConfiguration configuration = ValidateResource(resource);
        if (value > configuration.MaximumValue) throw new ArgumentOutOfRangeException(nameof(value));
        DacWrite(builder, resource, builder.Load(value));
    }
    private static DacResourceConfiguration ValidateResource(ResourceDescriptor resource) {
        DacResourceConfiguration configuration = resource.Configuration as DacResourceConfiguration ??
            throw new ArgumentException("DAC resource configuration is required.", nameof(resource));
        if (resource.ModuleId != ModuleId || resource.ResourceType != ResourceType) throw new ArgumentException("DAC resource type mismatch.", nameof(resource));
        if ((resource.Permissions & ResourcePermission.Write) == 0) throw new ArgumentException("DAC resource permission mismatch.", nameof(resource));
        if (configuration.MaximumValue == 0u || configuration.ResolutionBits is 0 or > 32) throw new ArgumentOutOfRangeException(nameof(resource));
        return configuration;
    }
}
