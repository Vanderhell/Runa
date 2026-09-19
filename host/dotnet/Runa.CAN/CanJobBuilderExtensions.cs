using System.Buffers.Binary;

namespace McuRemote;

public enum CanFilterMode : byte { Standard = 0, Extended = 1 }

public sealed record CanFrame(uint Id, bool Extended, byte[] Data);
public sealed record CanResourceConfiguration(
    byte Controller, CanFilterMode FilterMode, bool ListenOnly, uint Bitrate,
    uint AcceptId, uint AcceptMask, uint MaximumTimeoutUs);

public static class CanJobBuilderExtensions {
    public const ushort ModuleId = 7;
    public const ushort ResourceType = 1;
    public const byte TransmitOperation = 1;
    public const byte ReceiveOperation = 2;
    public const byte RequestResponseOperation = 3;
    public const ushort MaximumPayloadBytes = 8;
    public const uint MaximumTimeoutUs = 5_000_000;

    public static ModuleDescriptor Descriptor { get; } =
        new(ModuleId, 1, 1, [TransmitOperation, ReceiveOperation, RequestResponseOperation]);

    public static void CanTransmit(this JobBuilder builder, ResourceDescriptor resource,
                                   CanFrame frame, uint timeoutUs) {
        EmitFrame(builder, resource, frame, timeoutUs, TransmitOperation, ResourcePermission.Write);
    }

    public static void CanReceive(this JobBuilder builder, ResourceDescriptor resource, uint timeoutUs) {
        ArgumentNullException.ThrowIfNull(builder);
        CanResourceConfiguration configuration = ValidateResource(resource, ResourcePermission.Read);
        ValidateTimeout(configuration, timeoutUs);
        Span<byte> payload = stackalloc byte[6];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        BinaryPrimitives.WriteUInt32LittleEndian(payload[2..], timeoutUs);
        builder.EmitExtension(ModuleId, ReceiveOperation, payload);
    }

    public static void CanRequestResponse(this JobBuilder builder, ResourceDescriptor resource,
                                          CanFrame request, uint timeoutUs) {
        EmitFrame(builder, resource, request, timeoutUs, RequestResponseOperation,
            ResourcePermission.Read | ResourcePermission.Write);
    }

    private static void EmitFrame(JobBuilder builder, ResourceDescriptor resource, CanFrame frame,
                                  uint timeoutUs, byte operation, ResourcePermission permission) {
        ArgumentNullException.ThrowIfNull(builder);
        CanResourceConfiguration configuration = ValidateResource(resource, permission);
        ValidateFrame(frame);
        ValidateTimeout(configuration, timeoutUs);
        Span<byte> payload = stackalloc byte[12 + MaximumPayloadBytes];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        payload[2] = frame.Extended ? (byte)1 : (byte)0;
        BinaryPrimitives.WriteUInt32LittleEndian(payload[3..], frame.Id);
        payload[7] = checked((byte)frame.Data.Length);
        frame.Data.AsSpan().CopyTo(payload[8..]);
        BinaryPrimitives.WriteUInt32LittleEndian(payload[(8 + frame.Data.Length)..], timeoutUs);
        builder.EmitExtension(ModuleId, operation, payload[..(12 + frame.Data.Length)]);
    }

    private static CanResourceConfiguration ValidateResource(ResourceDescriptor resource, ResourcePermission permission) {
        CanResourceConfiguration configuration = resource.Configuration as CanResourceConfiguration ??
            throw new ArgumentException("CAN resource configuration is required.", nameof(resource));
        if (resource.ModuleId != ModuleId || resource.ResourceType != ResourceType)
            throw new ArgumentException("CAN resource type mismatch.", nameof(resource));
        if ((resource.Permissions & permission) != permission)
            throw new ArgumentException("CAN resource permission mismatch.", nameof(resource));
        uint limit = configuration.FilterMode == CanFilterMode.Standard ? 0x7ffu : 0x1fffffffu;
        if (configuration.Controller >= 2 || configuration.Bitrate is < 10_000 or > 1_000_000 ||
            configuration.MaximumTimeoutUs is 0 or > MaximumTimeoutUs || configuration.AcceptId > limit ||
            configuration.AcceptMask > limit)
            throw new ArgumentOutOfRangeException(nameof(configuration));
        return configuration;
    }

    private static void ValidateFrame(CanFrame frame) {
        ArgumentNullException.ThrowIfNull(frame);
        uint limit = frame.Extended ? 0x1fffffffu : 0x7ffu;
        if (frame.Id > limit || frame.Data.Length > MaximumPayloadBytes)
            throw new ArgumentOutOfRangeException(nameof(frame));
    }

    private static void ValidateTimeout(CanResourceConfiguration configuration, uint timeoutUs) {
        if (timeoutUs == 0 || timeoutUs > configuration.MaximumTimeoutUs)
            throw new ArgumentOutOfRangeException(nameof(timeoutUs));
    }
}

public static class CanModuleDataDecoder {
    public static CanFrame Decode(ModuleDataFrame frame) {
        if (frame.ModuleId != CanJobBuilderExtensions.ModuleId || frame.Data.Length < 6 ||
            frame.Data.Length > 14)
            throw new FormatException("Invalid CAN module data frame.");
        byte flags = frame.Data[0];
        uint id = BinaryPrimitives.ReadUInt32LittleEndian(frame.Data.AsSpan(1));
        byte length = frame.Data[5];
        if ((flags & 0xfe) != 0 || length > 8 || frame.Data.Length != 6 + length ||
            ((!((flags & 1) != 0)) && id > 0x7ff) || (((flags & 1) != 0) && id > 0x1fffffff))
            throw new FormatException("Invalid CAN frame payload.");
        return new(id, (flags & 1) != 0, frame.Data.AsSpan(6, length).ToArray());
    }
}
