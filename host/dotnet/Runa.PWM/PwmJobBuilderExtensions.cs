using System.Buffers.Binary;

namespace McuRemote;

public static class PwmJobBuilderExtensions {
    public const ushort ModuleId = 3;
    public const ushort ResourceType = 1;
    public const byte WriteOperation = 1;
    public static ModuleDescriptor Descriptor { get; } = new(ModuleId, 1, 1, [WriteOperation]);

    public static void PwmWrite(this JobBuilder builder, ResourceDescriptor resource, Register value) {
        if (resource.ModuleId != ModuleId || resource.ResourceType != ResourceType)
            throw new ArgumentException("PWM resource type mismatch.", nameof(resource));
        if ((resource.Permissions & ResourcePermission.Write) == 0)
            throw new ArgumentException("PWM resource permission mismatch.", nameof(resource));
        Span<byte> payload = stackalloc byte[3];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        payload[2] = value.Index;
        builder.EmitExtension(ModuleId, WriteOperation, payload);
    }
}
