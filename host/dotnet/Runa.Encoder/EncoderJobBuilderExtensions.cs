using System.Buffers.Binary;

namespace McuRemote;

public enum EncoderDecodeMode : byte { X4 = 4 }

public sealed record EncoderResourceConfiguration(
    short AInput, short BInput, EncoderDecodeMode DecodeMode = EncoderDecodeMode.X4,
    bool InvertDirection = false, int ResetValue = 0);

public static class EncoderJobBuilderExtensions {
    public const ushort ModuleId = 10;
    public const ushort ResourceType = 1;
    public const byte ReadOperation = 1;
    public const byte ResetOperation = 2;
    public const byte ReadResetOperation = 3;

    public static ModuleDescriptor Descriptor { get; } =
        new(ModuleId, 1, 1, [ReadOperation, ResetOperation, ReadResetOperation]);

    public static Register EncoderRead(this JobBuilder builder, ResourceDescriptor resource) {
        ValidateResource(resource, ResourcePermission.Read);
        Register destination = builder.Allocate();
        EmitResourceOperation(builder, resource, ReadOperation, destination.Index);
        return destination;
    }

    public static void EncoderReset(this JobBuilder builder, ResourceDescriptor resource) {
        ValidateResource(resource, ResourcePermission.Write);
        Span<byte> payload = stackalloc byte[2];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        builder.EmitExtension(ModuleId, ResetOperation, payload);
    }

    public static Register EncoderReadReset(this JobBuilder builder, ResourceDescriptor resource) {
        ValidateResource(resource, ResourcePermission.Read | ResourcePermission.Write);
        Register destination = builder.Allocate();
        EmitResourceOperation(builder, resource, ReadResetOperation, destination.Index);
        return destination;
    }

    public static int DecodeSignedPosition(uint registerBits) => unchecked((int)registerBits);

    private static void EmitResourceOperation(JobBuilder builder, ResourceDescriptor resource,
                                              byte operation, byte destination) {
        Span<byte> payload = stackalloc byte[3];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        payload[2] = destination;
        builder.EmitExtension(ModuleId, operation, payload);
    }

    private static void ValidateResource(ResourceDescriptor resource, ResourcePermission permission) {
        if (resource.ModuleId != ModuleId || resource.ResourceType != ResourceType)
            throw new ArgumentException("Encoder resource type mismatch.", nameof(resource));
        if ((resource.Permissions & permission) != permission)
            throw new ArgumentException("Encoder resource permission mismatch.", nameof(resource));
        EncoderResourceConfiguration configuration = resource.Configuration as EncoderResourceConfiguration ??
            throw new ArgumentException("Encoder resource configuration is required.", nameof(resource));
        if (configuration.AInput < 0 || configuration.BInput < 0 ||
            configuration.AInput == configuration.BInput || configuration.DecodeMode != EncoderDecodeMode.X4)
            throw new ArgumentOutOfRangeException(nameof(resource));
    }
}
