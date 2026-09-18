using System.Buffers.Binary;

namespace McuRemote;

public static class AdcJobBuilderExtensions {
    public const ushort ModuleId = 2;
    public const ushort ResourceType = 1;
    public const byte ReadOperation = 1;
    public static ModuleDescriptor Descriptor { get; } = new(ModuleId, 1, 1, [ReadOperation]);

    public static Register AdcRead(this JobBuilder builder, ResourceDescriptor resource) {
        if (resource.ModuleId != ModuleId || resource.ResourceType != ResourceType)
            throw new ArgumentException("ADC resource type mismatch.", nameof(resource));
        if ((resource.Permissions & ResourcePermission.Read) == 0)
            throw new ArgumentException("ADC resource permission mismatch.", nameof(resource));
        Register destination = builder.Allocate();
        Span<byte> payload = stackalloc byte[3];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        payload[2] = destination.Index;
        builder.EmitExtension(ModuleId, ReadOperation, payload);
        return destination;
    }
}
