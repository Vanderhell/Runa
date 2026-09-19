using System.Buffers.Binary;

namespace McuRemote;

[Flags]
public enum WatchdogStatusFlags : byte {
    Supported = 1,
    Armed = 2,
    DisarmSupported = 4,
    Windowed = 8,
    LastResetWatchdog = 16,
    TimeoutConfigured = 32
}

public sealed record WatchdogResourceConfiguration(
    uint MinimumTimeoutMs, uint MaximumTimeoutMs, uint DefaultTimeoutMs = 0,
    bool StatusSupported = true, bool ArmSupported = true, bool FeedSupported = true,
    bool DisarmSupported = false, bool Windowed = false, bool LastResetSupported = false);

public readonly record struct WatchdogStatus(WatchdogStatusFlags Flags, uint TimeoutMs) {
    public bool Armed => (Flags & WatchdogStatusFlags.Armed) != 0;
    public bool DisarmSupported => (Flags & WatchdogStatusFlags.DisarmSupported) != 0;
    public bool LastResetWasWatchdog => (Flags & WatchdogStatusFlags.LastResetWatchdog) != 0;
}

public static class WatchdogJobBuilderExtensions {
    public const ushort ModuleId = 14;
    public const ushort ResourceType = 1;
    public const byte GetStatusOperation = 1;
    public const byte ArmOperation = 2;
    public const byte FeedOperation = 3;
    public const byte DisarmOperation = 4;
    public const uint MaximumTimeoutMs = 86_400_000;

    public static ModuleDescriptor Descriptor { get; } =
        new(ModuleId, 1, 1, [GetStatusOperation, ArmOperation, FeedOperation, DisarmOperation]);

    public static void WatchdogGetStatus(this JobBuilder builder, ResourceDescriptor resource) {
        ArgumentNullException.ThrowIfNull(builder);
        WatchdogResourceConfiguration configuration = ValidateResource(resource, ResourcePermission.Read);
        if (!configuration.StatusSupported)
            throw new ArgumentException("Watchdog STATUS is unsupported.", nameof(resource));
        EmitResourceOperation(builder, resource, GetStatusOperation);
    }

    public static void WatchdogArm(this JobBuilder builder, ResourceDescriptor resource,
                                   uint timeoutMs) {
        ArgumentNullException.ThrowIfNull(builder);
        WatchdogResourceConfiguration configuration = ValidateResource(resource, ResourcePermission.Write);
        if (!configuration.ArmSupported) throw new ArgumentException("Watchdog ARM is unsupported.", nameof(resource));
        ValidateTimeout(configuration, timeoutMs);
        Span<byte> payload = stackalloc byte[6];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        BinaryPrimitives.WriteUInt32LittleEndian(payload[2..], timeoutMs);
        builder.EmitExtension(ModuleId, ArmOperation, payload);
    }

    public static void WatchdogFeed(this JobBuilder builder, ResourceDescriptor resource) {
        ArgumentNullException.ThrowIfNull(builder);
        WatchdogResourceConfiguration configuration = ValidateResource(resource, ResourcePermission.Write);
        if (!configuration.FeedSupported) throw new ArgumentException("Watchdog FEED is unsupported.", nameof(resource));
        EmitResourceOperation(builder, resource, FeedOperation);
    }

    public static void WatchdogDisarm(this JobBuilder builder, ResourceDescriptor resource) {
        ArgumentNullException.ThrowIfNull(builder);
        WatchdogResourceConfiguration configuration = ValidateResource(resource, ResourcePermission.Write);
        if (!configuration.DisarmSupported) throw new ArgumentException("Watchdog DISARM is unsupported.", nameof(resource));
        EmitResourceOperation(builder, resource, DisarmOperation);
    }

    public static WatchdogStatus DecodeStatus(ReadOnlySpan<byte> data) {
        if (data.Length != 8 || data[0] != 1 || data[2] != 0 || data[3] != 0 ||
            (data[1] & 0xc0) != 0)
            throw new FormatException("Invalid Watchdog status data.");
        return new((WatchdogStatusFlags)data[1], BinaryPrimitives.ReadUInt32LittleEndian(data[4..]));
    }

    private static WatchdogResourceConfiguration ValidateResource(ResourceDescriptor resource,
                                                                   ResourcePermission permission) {
        if (resource.ModuleId != ModuleId || resource.ResourceType != ResourceType)
            throw new ArgumentException("Watchdog resource type mismatch.", nameof(resource));
        if ((resource.Permissions & permission) != permission)
            throw new ArgumentException("Watchdog resource permission mismatch.", nameof(resource));
        WatchdogResourceConfiguration configuration = Configuration(resource);
        if (configuration.MinimumTimeoutMs == 0 ||
            configuration.MaximumTimeoutMs < configuration.MinimumTimeoutMs ||
            configuration.MaximumTimeoutMs > MaximumTimeoutMs ||
            configuration.DefaultTimeoutMs != 0 &&
                (configuration.DefaultTimeoutMs < configuration.MinimumTimeoutMs ||
                 configuration.DefaultTimeoutMs > configuration.MaximumTimeoutMs) ||
            configuration.Windowed)
            throw new ArgumentOutOfRangeException(nameof(resource));
        return configuration;
    }

    private static WatchdogResourceConfiguration Configuration(ResourceDescriptor resource) =>
        resource.Configuration as WatchdogResourceConfiguration ??
        throw new ArgumentException("Watchdog resource configuration is required.", nameof(resource));

    private static void ValidateTimeout(WatchdogResourceConfiguration configuration, uint timeoutMs) {
        if (timeoutMs < configuration.MinimumTimeoutMs || timeoutMs > configuration.MaximumTimeoutMs)
            throw new ArgumentOutOfRangeException(nameof(timeoutMs));
    }

    private static void EmitResourceOperation(JobBuilder builder, ResourceDescriptor resource,
                                              byte operation) {
        Span<byte> payload = stackalloc byte[2];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        builder.EmitExtension(ModuleId, operation, payload);
    }
}
