using System.Buffers.Binary;

namespace McuRemote;

public sealed record I2cResourceConfiguration(
    byte Bus, byte Address, ushort MaximumTxBytes, ushort MaximumRxBytes,
    uint ClockHz, ushort MaximumTimeoutMs);

public static class I2cJobBuilderExtensions {
    public const ushort ModuleId = 5;
    public const ushort ResourceType = 1;
    public const byte TransferOperation = 1;
    public const ushort MaximumTxBytes = 246;
    public const ushort MaximumRxBytes = 246;

    public static ModuleDescriptor Descriptor { get; } = new(ModuleId, 1, 1, [TransferOperation]);

    public static void I2cTransfer(this JobBuilder builder, ResourceDescriptor resource,
                                   ReadOnlySpan<byte> transmit, byte receiveLength, ushort timeoutMs) {
        ArgumentNullException.ThrowIfNull(builder);
        I2cResourceConfiguration configuration = resource.Configuration as I2cResourceConfiguration ??
            throw new ArgumentException("I2C resource configuration is required.", nameof(resource));
        if (resource.ModuleId != ModuleId || resource.ResourceType != ResourceType)
            throw new ArgumentException("I2C resource type mismatch.", nameof(resource));
        if ((transmit.Length == 0 && receiveLength == 0) || transmit.Length > MaximumTxBytes ||
            receiveLength > MaximumRxBytes)
            throw new ArgumentOutOfRangeException(nameof(transmit));
        if ((transmit.Length != 0 && (resource.Permissions & ResourcePermission.Write) == 0) ||
            (receiveLength != 0 && (resource.Permissions & ResourcePermission.Read) == 0))
            throw new ArgumentException("I2C resource permission mismatch.", nameof(resource));
        if (configuration.Address == 0 || configuration.Address > 0x7f ||
            configuration.MaximumTxBytes is 0 or > MaximumTxBytes ||
            configuration.MaximumRxBytes is 0 or > MaximumRxBytes ||
            configuration.ClockHz is < 10_000 or > 1_000_000 ||
            configuration.MaximumTimeoutMs is 0 or > 5000 || timeoutMs is 0 ||
            timeoutMs > configuration.MaximumTimeoutMs || transmit.Length > configuration.MaximumTxBytes ||
            receiveLength > configuration.MaximumRxBytes)
            throw new ArgumentOutOfRangeException(nameof(configuration));

        Span<byte> payload = stackalloc byte[6 + MaximumTxBytes];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        payload[2] = checked((byte)transmit.Length);
        payload[3] = receiveLength;
        BinaryPrimitives.WriteUInt16LittleEndian(payload[4..], timeoutMs);
        transmit.CopyTo(payload[6..]);
        builder.EmitExtension(ModuleId, TransferOperation, payload[..(6 + transmit.Length)]);
    }
}
