using System.Buffers.Binary;

namespace McuRemote;

[Flags]
public enum RtcStatusFlags : uint {
    ValidTime = 1,
    PowerLossDetected = 2,
    OscillatorStopped = 4,
    BatteryBacked = 8,
    SetSupported = 16
}

public sealed record RtcResourceConfiguration(
    ulong MinimumSeconds = RunaRtcTime.MinimumSeconds,
    ulong MaximumSeconds = RunaRtcTime.MaximumSeconds,
    bool SetSupported = false,
    bool BatteryBacked = false,
    bool RetainedAcrossReset = false,
    bool RetainedAcrossDeepSleep = false,
    bool RetainedAcrossPowerLoss = false,
    RtcStatusFlags SupportedStatusFlags = RtcStatusFlags.ValidTime |
        RtcStatusFlags.PowerLossDetected | RtcStatusFlags.OscillatorStopped);

public readonly record struct RunaRtcTime {
    public const ulong MinimumSeconds = 0;
    public const ulong MaximumSeconds = 253402300799;
    public ulong Seconds { get; }

    public RunaRtcTime(ulong seconds) {
        if (seconds > MaximumSeconds) throw new ArgumentOutOfRangeException(nameof(seconds));
        Seconds = seconds;
    }

    public static RunaRtcTime FromDateTimeOffset(DateTimeOffset utc) {
        if (utc.Offset != TimeSpan.Zero) throw new ArgumentException("UTC is required.", nameof(utc));
        TimeSpan delta = utc - DateTimeOffset.UnixEpoch;
        if (delta < TimeSpan.Zero || delta.Ticks % TimeSpan.TicksPerSecond != 0)
            throw new ArgumentOutOfRangeException(nameof(utc), "RTC resolution is one second.");
        return new RunaRtcTime(checked((ulong)(delta.Ticks / TimeSpan.TicksPerSecond)));
    }

    public DateTimeOffset ToDateTimeOffset() {
        long ticks = checked((long)Seconds * TimeSpan.TicksPerSecond);
        return DateTimeOffset.UnixEpoch.AddTicks(ticks);
    }
}

public sealed record RtcTimeResult(RunaRtcTime Time, RtcStatusFlags StatusFlags);

public static class RtcJobBuilderExtensions {
    public const ushort ModuleId = 13;
    public const ushort ResourceType = 1;
    public const byte ReadOperation = 1;
    public const byte SetOperation = 2;
    public const byte GetStatusOperation = 3;

    public static ModuleDescriptor Descriptor { get; } =
        new(ModuleId, 1, 1, [ReadOperation, SetOperation, GetStatusOperation]);

    public static void RtcRead(this JobBuilder builder, ResourceDescriptor resource) {
        ArgumentNullException.ThrowIfNull(builder);
        ValidateResource(resource, ResourcePermission.Read);
        Span<byte> payload = stackalloc byte[2];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        builder.EmitExtension(ModuleId, ReadOperation, payload);
    }

    public static void RtcSet(this JobBuilder builder, ResourceDescriptor resource,
                              RunaRtcTime time) {
        ArgumentNullException.ThrowIfNull(builder);
        RtcResourceConfiguration configuration = ValidateResource(resource, ResourcePermission.Write);
        if (!configuration.SetSupported)
            throw new ArgumentException("RTC SET is unsupported.", nameof(resource));
        if (time.Seconds < configuration.MinimumSeconds ||
            time.Seconds > configuration.MaximumSeconds)
            throw new ArgumentOutOfRangeException(nameof(time));
        Span<byte> payload = stackalloc byte[10];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        BinaryPrimitives.WriteUInt64LittleEndian(payload[2..], time.Seconds);
        builder.EmitExtension(ModuleId, SetOperation, payload);
    }

    public static void RtcGetStatus(this JobBuilder builder, ResourceDescriptor resource) {
        ArgumentNullException.ThrowIfNull(builder);
        ValidateResource(resource, ResourcePermission.Read);
        Span<byte> payload = stackalloc byte[2];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        builder.EmitExtension(ModuleId, GetStatusOperation, payload);
    }

    private static RtcResourceConfiguration ValidateResource(ResourceDescriptor resource,
                                                              ResourcePermission permission) {
        RtcResourceConfiguration configuration = resource.Configuration as RtcResourceConfiguration ??
            throw new ArgumentException("RTC resource configuration is required.", nameof(resource));
        if (resource.ModuleId != ModuleId || resource.ResourceType != ResourceType)
            throw new ArgumentException("RTC resource type mismatch.", nameof(resource));
        if ((resource.Permissions & permission) != permission)
            throw new ArgumentException("RTC resource permission mismatch.", nameof(resource));
        RtcStatusFlags knownFlags = RtcStatusFlags.ValidTime | RtcStatusFlags.PowerLossDetected |
            RtcStatusFlags.OscillatorStopped;
        if (configuration.MinimumSeconds > configuration.MaximumSeconds ||
            configuration.MaximumSeconds > RunaRtcTime.MaximumSeconds ||
            (!configuration.BatteryBacked && configuration.RetainedAcrossPowerLoss) ||
            (configuration.SupportedStatusFlags & ~knownFlags) != 0)
            throw new ArgumentOutOfRangeException(nameof(configuration));
        return configuration;
    }
}

public static class RtcModuleDataDecoder {
    private const RtcStatusFlags KnownStatusFlags = RtcStatusFlags.ValidTime |
        RtcStatusFlags.PowerLossDetected | RtcStatusFlags.OscillatorStopped |
        RtcStatusFlags.BatteryBacked | RtcStatusFlags.SetSupported;

    public static RtcTimeResult DecodeRead(ModuleDataFrame frame) {
        if (frame.ModuleId != RtcJobBuilderExtensions.ModuleId || frame.Data.Length != 16 ||
            frame.Data[0] != 1 || frame.Data[1] != 1 || frame.Data[2] != 0 || frame.Data[3] != 0)
            throw new FormatException("Invalid RTC read module data.");
        ulong seconds = BinaryPrimitives.ReadUInt64LittleEndian(frame.Data.AsSpan(4, 8));
        RtcStatusFlags flags = (RtcStatusFlags)BinaryPrimitives.ReadUInt32LittleEndian(
            frame.Data.AsSpan(12, 4));
        ValidateStatusFlags(flags);
        return new(new RunaRtcTime(seconds), flags);
    }

    public static RtcStatusFlags DecodeStatus(ModuleDataFrame frame) {
        if (frame.ModuleId != RtcJobBuilderExtensions.ModuleId || frame.Data.Length != 8 ||
            frame.Data[0] != 1 || frame.Data[1] != 2 || frame.Data[2] != 0 || frame.Data[3] != 0)
            throw new FormatException("Invalid RTC status module data.");
        RtcStatusFlags flags = (RtcStatusFlags)BinaryPrimitives.ReadUInt32LittleEndian(
            frame.Data.AsSpan(4, 4));
        ValidateStatusFlags(flags);
        return flags;
    }

    private static void ValidateStatusFlags(RtcStatusFlags flags) {
        if ((flags & ~KnownStatusFlags) != 0)
            throw new FormatException("Unknown RTC status flags.");
    }
}
