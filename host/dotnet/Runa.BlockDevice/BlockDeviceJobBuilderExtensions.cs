using System.Buffers.Binary;

namespace McuRemote;

[Flags]
public enum BlockDeviceCapabilities : byte {
    Read = 1,
    Write = 2,
    Erase = 4,
    Sync = 8,
    EraseRequiredBeforeWrite = 16,
    Persistent = 32
}

public sealed record BlockDeviceResourceConfiguration(
    uint Capacity,
    uint ReadAlignment,
    uint WriteAlignment,
    uint EraseAlignment,
    ushort MaxReadBytes,
    ushort MaxWriteBytes,
    ushort MaxEraseBytes,
    byte ErasedValue,
    BlockDeviceCapabilities Capabilities,
    ushort AtomicWriteSize = 0);

public static class BlockDeviceJobBuilderExtensions {
    public const ushort ModuleId = 12;
    public const ushort ResourceType = 1;
    public const byte ReadOperation = 1;
    public const byte WriteOperation = 2;
    public const byte EraseOperation = 3;
    public const byte SyncOperation = 4;
    public const uint MaximumCapacity = uint.MaxValue;
    public const ushort MaximumReadBytes = 192;
    public const ushort MaximumWriteBytes = 192;
    public const ushort MaximumEraseBytes = 4096;

    public static ModuleDescriptor Descriptor { get; } =
        new(ModuleId, 1, 1, [ReadOperation, WriteOperation, EraseOperation, SyncOperation]);

    public static void BlockRead(this JobBuilder builder, ResourceDescriptor resource,
                                 uint offset, uint length) {
        ArgumentNullException.ThrowIfNull(builder);
        BlockDeviceResourceConfiguration configuration = ValidateResource(resource);
        ValidateOperation(resource, configuration, ResourcePermission.Read,
            BlockDeviceCapabilities.Read, offset, length, configuration.ReadAlignment,
            configuration.MaxReadBytes);
        Span<byte> payload = stackalloc byte[10];
        WriteRange(payload, resource.Id, offset, length);
        builder.EmitExtension(ModuleId, ReadOperation, payload);
    }

    public static void BlockWrite(this JobBuilder builder, ResourceDescriptor resource,
                                  uint offset, ReadOnlySpan<byte> data) {
        ArgumentNullException.ThrowIfNull(builder);
        BlockDeviceResourceConfiguration configuration = ValidateResource(resource);
        uint length = checked((uint)data.Length);
        ValidateOperation(resource, configuration, ResourcePermission.Write,
            BlockDeviceCapabilities.Write, offset, length, configuration.WriteAlignment,
            configuration.MaxWriteBytes);
        Span<byte> payload = stackalloc byte[10 + MaximumWriteBytes];
        WriteRange(payload, resource.Id, offset, length);
        data.CopyTo(payload[10..]);
        builder.EmitExtension(ModuleId, WriteOperation, payload[..checked(10 + data.Length)]);
    }

    public static void BlockErase(this JobBuilder builder, ResourceDescriptor resource,
                                  uint offset, uint length) {
        ArgumentNullException.ThrowIfNull(builder);
        BlockDeviceResourceConfiguration configuration = ValidateResource(resource);
        ValidateOperation(resource, configuration, ResourcePermission.Write,
            BlockDeviceCapabilities.Erase, offset, length, configuration.EraseAlignment,
            configuration.MaxEraseBytes);
        Span<byte> payload = stackalloc byte[10];
        WriteRange(payload, resource.Id, offset, length);
        builder.EmitExtension(ModuleId, EraseOperation, payload);
    }

    public static void BlockSync(this JobBuilder builder, ResourceDescriptor resource) {
        ArgumentNullException.ThrowIfNull(builder);
        BlockDeviceResourceConfiguration configuration = ValidateResource(resource);
        if ((resource.Permissions & ResourcePermission.Write) == 0 ||
            !configuration.Capabilities.HasFlag(BlockDeviceCapabilities.Sync))
            throw new ArgumentException("BlockDevice sync permission or capability is missing.",
                nameof(resource));
        Span<byte> payload = stackalloc byte[2];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        builder.EmitExtension(ModuleId, SyncOperation, payload);
    }

    private static BlockDeviceResourceConfiguration ValidateResource(ResourceDescriptor resource) {
        BlockDeviceResourceConfiguration configuration =
            resource.Configuration as BlockDeviceResourceConfiguration ??
            throw new ArgumentException("BlockDevice resource configuration is required.", nameof(resource));
        const BlockDeviceCapabilities allCapabilities =
            BlockDeviceCapabilities.Read | BlockDeviceCapabilities.Write |
            BlockDeviceCapabilities.Erase | BlockDeviceCapabilities.Sync |
            BlockDeviceCapabilities.EraseRequiredBeforeWrite | BlockDeviceCapabilities.Persistent;
        if (resource.ModuleId != ModuleId || resource.ResourceType != ResourceType)
            throw new ArgumentException("BlockDevice resource type mismatch.", nameof(resource));
        if (configuration.Capacity == 0 || configuration.ReadAlignment == 0 ||
            configuration.WriteAlignment == 0 || configuration.EraseAlignment == 0 ||
            (configuration.Capabilities & ~allCapabilities) != 0 ||
            configuration.MaxReadBytes > MaximumReadBytes ||
            configuration.MaxWriteBytes > MaximumWriteBytes ||
            configuration.MaxEraseBytes > MaximumEraseBytes ||
            (configuration.Capabilities.HasFlag(BlockDeviceCapabilities.Read) &&
             configuration.MaxReadBytes == 0) ||
            (configuration.Capabilities.HasFlag(BlockDeviceCapabilities.Write) &&
             configuration.MaxWriteBytes == 0) ||
            (configuration.Capabilities.HasFlag(BlockDeviceCapabilities.Erase) &&
             configuration.MaxEraseBytes == 0) ||
            (configuration.AtomicWriteSize != 0 &&
             (!configuration.Capabilities.HasFlag(BlockDeviceCapabilities.Write) ||
              configuration.AtomicWriteSize > configuration.MaxWriteBytes)))
            throw new ArgumentOutOfRangeException(nameof(resource));
        return configuration;
    }

    private static void ValidateOperation(ResourceDescriptor resource,
                                           BlockDeviceResourceConfiguration configuration,
                                           ResourcePermission permission,
                                           BlockDeviceCapabilities capability,
                                           uint offset, uint length, uint alignment,
                                           ushort maximum) {
        if ((resource.Permissions & permission) != permission)
            throw new ArgumentException("BlockDevice resource permission mismatch.", nameof(resource));
        if (!configuration.Capabilities.HasFlag(capability))
            throw new NotSupportedException("BlockDevice operation is not supported by the resource.");
        if (length == 0 || offset > configuration.Capacity ||
            length > configuration.Capacity - offset || length > maximum ||
            offset % alignment != 0 || length % alignment != 0)
            throw new ArgumentOutOfRangeException(nameof(length));
    }

    private static void WriteRange(Span<byte> payload, ushort resourceId, uint offset, uint length) {
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resourceId);
        BinaryPrimitives.WriteUInt32LittleEndian(payload[2..], offset);
        BinaryPrimitives.WriteUInt32LittleEndian(payload[6..], length);
    }
}
