using System.Buffers.Binary;

namespace McuRemote;

public enum UartReceivePolicy : byte { FixedLength = 0, UpToLength = 1 }
public enum UartParity : byte { None = 0, Even = 1, Odd = 2 }
public enum UartFlowControl : byte { None = 0, Rts = 1, Cts = 2, RtsCts = 3 }

public sealed record UartResourceConfiguration(
    byte Controller, byte DataBits, byte StopBits, UartParity Parity,
    UartFlowControl FlowControl, short TxPin, short RxPin, uint BaudRate,
    ushort MaximumTxBytes, ushort MaximumRxBytes, uint MaximumTimeoutUs);

public static class UartJobBuilderExtensions {
    public const ushort ModuleId = 6;
    public const ushort ResourceType = 1;
    public const byte TransferOperation = 1;
    public const ushort MaximumTxBytes = 240;
    public const ushort MaximumRxBytes = 240;
    public const uint MaximumTimeoutUs = 5_000_000;

    public static ModuleDescriptor Descriptor { get; } = new(ModuleId, 1, 1, [TransferOperation]);

    public static void UartTransfer(this JobBuilder builder, ResourceDescriptor resource,
                                    ReadOnlySpan<byte> transmit, byte receiveLength,
                                    UartReceivePolicy policy, uint timeoutUs) {
        ArgumentNullException.ThrowIfNull(builder);
        UartResourceConfiguration configuration = resource.Configuration as UartResourceConfiguration ??
            throw new ArgumentException("UART resource configuration is required.", nameof(resource));
        if (resource.ModuleId != ModuleId || resource.ResourceType != ResourceType)
            throw new ArgumentException("UART resource type mismatch.", nameof(resource));
        if ((transmit.Length == 0 && receiveLength == 0) || transmit.Length > MaximumTxBytes ||
            receiveLength > MaximumRxBytes)
            throw new ArgumentOutOfRangeException(nameof(transmit));
        if ((transmit.Length != 0 && (resource.Permissions & ResourcePermission.Write) == 0) ||
            (receiveLength != 0 && (resource.Permissions & ResourcePermission.Read) == 0))
            throw new ArgumentException("UART resource permission mismatch.", nameof(resource));
        if (configuration.DataBits is < 5 or > 8 || configuration.StopBits is < 1 or > 2 ||
            configuration.Parity > UartParity.Odd || configuration.FlowControl > UartFlowControl.RtsCts ||
            configuration.TxPin < 0 || configuration.RxPin < 0 || configuration.BaudRate is < 300 or > 5_000_000 ||
            configuration.MaximumTxBytes is 0 or > MaximumTxBytes ||
            configuration.MaximumRxBytes is 0 or > MaximumRxBytes ||
            configuration.MaximumTimeoutUs is 0 or > MaximumTimeoutUs ||
            transmit.Length > configuration.MaximumTxBytes || receiveLength > configuration.MaximumRxBytes ||
            timeoutUs > configuration.MaximumTimeoutUs ||
            (receiveLength != 0 && timeoutUs == 0) ||
            (receiveLength == 0 && policy != UartReceivePolicy.FixedLength))
            throw new ArgumentOutOfRangeException(nameof(configuration));

        Span<byte> payload = stackalloc byte[9 + MaximumTxBytes];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        payload[2] = checked((byte)transmit.Length);
        payload[3] = receiveLength;
        payload[4] = (byte)policy;
        BinaryPrimitives.WriteUInt32LittleEndian(payload[5..], timeoutUs);
        transmit.CopyTo(payload[9..]);
        builder.EmitExtension(ModuleId, TransferOperation, payload[..(9 + transmit.Length)]);
    }
}
