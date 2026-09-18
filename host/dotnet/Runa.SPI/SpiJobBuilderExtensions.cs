using System.Buffers.Binary;

namespace McuRemote;

public enum SpiChipSelectBehavior : byte { Automatic = 0, Software = 1 }

public sealed record SpiResourceConfiguration(
    byte Mode, SpiChipSelectBehavior ChipSelectBehavior, ushort MaximumTransferBytes,
    uint ClockHz, uint MinimumClockHz, uint MaximumClockHz, ushort MaximumTimeoutMs,
    nuint ChipSelectHandle = 0);

public static class SpiJobBuilderExtensions {
    public const ushort ModuleId = 4;
    public const ushort ResourceType = 1;
    public const byte TransferOperation = 1;
    public const ushort MaximumTransferBytes = 246;

    public static ModuleDescriptor Descriptor { get; } = new(ModuleId, 1, 1, [TransferOperation]);

    public static void SpiTransfer(this JobBuilder builder, ResourceDescriptor resource,
                                   ReadOnlySpan<byte> transmit, byte receiveSize,
                                   ushort timeoutMs) {
        ArgumentNullException.ThrowIfNull(builder);
        SpiResourceConfiguration configuration = resource.Configuration as SpiResourceConfiguration ??
            throw new ArgumentException("SPI resource configuration is required.", nameof(resource));
        if (resource.ModuleId != ModuleId || resource.ResourceType != ResourceType)
            throw new ArgumentException("SPI resource type mismatch.", nameof(resource));
        if ((transmit.Length == 0 && receiveSize == 0) || transmit.Length > MaximumTransferBytes ||
            receiveSize > MaximumTransferBytes)
            throw new ArgumentOutOfRangeException(nameof(transmit));
        if ((transmit.Length != 0 && (resource.Permissions & ResourcePermission.Write) == 0) ||
            (receiveSize != 0 && (resource.Permissions & ResourcePermission.Read) == 0))
            throw new ArgumentException("SPI resource permission mismatch.", nameof(resource));
        if (configuration.Mode > 3 || configuration.ChipSelectBehavior > SpiChipSelectBehavior.Software ||
            configuration.MaximumTransferBytes is 0 or > MaximumTransferBytes ||
            configuration.MinimumClockHz == 0 || configuration.MinimumClockHz > configuration.ClockHz ||
            configuration.ClockHz > configuration.MaximumClockHz ||
            configuration.MaximumTimeoutMs is 0 or > 5000 || timeoutMs is 0 ||
            timeoutMs > configuration.MaximumTimeoutMs ||
            Math.Max(transmit.Length, receiveSize) > configuration.MaximumTransferBytes)
            throw new ArgumentOutOfRangeException(nameof(configuration));
        ulong transferBytes = (ulong)Math.Max(transmit.Length, receiveSize);
        ulong wireTimeUs = (transferBytes * 8_000_000UL + configuration.ClockHz - 1UL) /
                           configuration.ClockHz;
        ulong minimumTimeoutMs = (wireTimeUs + 999UL) / 1000UL + 1UL;
        if (timeoutMs < minimumTimeoutMs)
            throw new ArgumentOutOfRangeException(nameof(timeoutMs));

        Span<byte> payload = stackalloc byte[6 + MaximumTransferBytes];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        payload[2] = checked((byte)transmit.Length);
        payload[3] = receiveSize;
        BinaryPrimitives.WriteUInt16LittleEndian(payload[4..], timeoutMs);
        transmit.CopyTo(payload[6..]);
        builder.EmitExtension(ModuleId, TransferOperation, payload[..(6 + transmit.Length)]);
    }
}
