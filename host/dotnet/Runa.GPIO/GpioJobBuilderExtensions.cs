using System.Buffers.Binary;

namespace McuRemote;

public static class GpioJobBuilderExtensions {
    public const ushort ModuleId = 1;
    public const ushort ResourceType = 1;
    public const byte ReadOperation = 1;
    public const byte WriteOperation = 2;
    public static ModuleDescriptor Descriptor { get; } = new(ModuleId, 1, 1,
        [ReadOperation, WriteOperation]);

    public static Register GpioRead(this JobBuilder builder, ResourceDescriptor resource) {
        Need(resource, ResourcePermission.Read);
        Register destination = builder.Allocate();
        Span<byte> payload = stackalloc byte[3];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        payload[2] = destination.Index;
        builder.EmitExtension(ModuleId, ReadOperation, payload);
        return destination;
    }

    public static void GpioWrite(this JobBuilder builder, ResourceDescriptor resource, Register value) {
        Need(resource, ResourcePermission.Write);
        Span<byte> payload = stackalloc byte[3];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        payload[2] = value.Index;
        builder.EmitExtension(ModuleId, WriteOperation, payload);
    }

    private static void Need(ResourceDescriptor resource, ResourcePermission permission) {
        if (resource.ModuleId != ModuleId || resource.ResourceType != ResourceType)
            throw new ArgumentException("GPIO resource type mismatch.", nameof(resource));
        if ((resource.Permissions & permission) != permission)
            throw new ArgumentException("GPIO resource permission mismatch.", nameof(resource));
    }
}
