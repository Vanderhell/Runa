using System.Buffers.Binary;

namespace McuRemote;

public enum PulseEdge : byte { Rising = 0, Falling = 1, Both = 2 }
public enum PulseLevel : byte { High = 1, Low = 2 }

public sealed record PulseResourceConfiguration(
    uint MinimumWindowUs, uint MaximumWindowUs, uint MinimumTimeoutUs,
    uint MaximumTimeoutUs, uint MaximumCount, byte SupportedEdges, byte SupportedLevels,
    bool GlitchFilterSupported = false);

public static class PulseJobBuilderExtensions {
    public const ushort ModuleId = 8;
    public const ushort ResourceType = 1;
    public const byte CountOperation = 1;
    public const byte MeasureWidthOperation = 2;
    public const byte MeasurePeriodOperation = 3;
    public const uint MaximumWindowUs = 5_000_000;
    public const uint MaximumTimeoutUs = 5_000_000;
    public const uint MaximumCount = 1_000_000;

    public static ModuleDescriptor Descriptor { get; } =
        new(ModuleId, 1, 1, [CountOperation, MeasureWidthOperation, MeasurePeriodOperation]);

    public static Register PulseCount(this JobBuilder builder, ResourceDescriptor resource,
                                      PulseEdge edge, uint windowUs, uint maxCount) {
        ArgumentNullException.ThrowIfNull(builder);
        PulseResourceConfiguration configuration = ValidateResource(resource);
        uint edgeBit = 1u << (int)edge;
        if ((configuration.SupportedEdges & edgeBit) == 0u ||
            windowUs < configuration.MinimumWindowUs || windowUs > configuration.MaximumWindowUs ||
            windowUs > MaximumWindowUs || maxCount == 0u || maxCount > configuration.MaximumCount ||
            maxCount > MaximumCount)
            throw new ArgumentOutOfRangeException(nameof(windowUs));
        Register destination = builder.Allocate();
        Span<byte> payload = stackalloc byte[12];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        payload[2] = (byte)edge;
        BinaryPrimitives.WriteUInt32LittleEndian(payload[3..], windowUs);
        BinaryPrimitives.WriteUInt32LittleEndian(payload[7..], maxCount);
        payload[11] = destination.Index;
        builder.EmitExtension(ModuleId, CountOperation, payload);
        return destination;
    }

    public static Register MeasurePulseWidth(this JobBuilder builder, ResourceDescriptor resource,
                                             PulseLevel level, uint timeoutUs) {
        ArgumentNullException.ThrowIfNull(builder);
        PulseResourceConfiguration configuration = ValidateResource(resource);
        if ((configuration.SupportedLevels & (byte)level) == 0u ||
            timeoutUs < configuration.MinimumTimeoutUs || timeoutUs > configuration.MaximumTimeoutUs ||
            timeoutUs > MaximumTimeoutUs)
            throw new ArgumentOutOfRangeException(nameof(timeoutUs));
        return EmitTimed(builder, resource, (byte)level, timeoutUs, MeasureWidthOperation);
    }

    public static Register MeasurePulsePeriod(this JobBuilder builder, ResourceDescriptor resource,
                                              PulseEdge edge, uint timeoutUs) {
        ArgumentNullException.ThrowIfNull(builder);
        PulseResourceConfiguration configuration = ValidateResource(resource);
        if (edge == PulseEdge.Both || (configuration.SupportedEdges & (1u << (int)edge)) == 0u ||
            timeoutUs < configuration.MinimumTimeoutUs || timeoutUs > configuration.MaximumTimeoutUs ||
            timeoutUs > MaximumTimeoutUs)
            throw new ArgumentOutOfRangeException(nameof(timeoutUs));
        return EmitTimed(builder, resource, (byte)edge, timeoutUs, MeasurePeriodOperation);
    }

    public static double FrequencyHz(uint periodUs) {
        if (periodUs == 0u) throw new ArgumentOutOfRangeException(nameof(periodUs));
        return 1_000_000d / periodUs;
    }

    private static Register EmitTimed(JobBuilder builder, ResourceDescriptor resource,
                                      byte selector, uint timeoutUs, byte operation) {
        Register destination = builder.Allocate();
        Span<byte> payload = stackalloc byte[8];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        payload[2] = selector;
        BinaryPrimitives.WriteUInt32LittleEndian(payload[3..], timeoutUs);
        payload[7] = destination.Index;
        builder.EmitExtension(ModuleId, operation, payload);
        return destination;
    }

    private static PulseResourceConfiguration ValidateResource(ResourceDescriptor resource) {
        PulseResourceConfiguration configuration = resource.Configuration as PulseResourceConfiguration ??
            throw new ArgumentException("Pulse resource configuration is required.", nameof(resource));
        if (resource.ModuleId != ModuleId || resource.ResourceType != ResourceType)
            throw new ArgumentException("Pulse resource type mismatch.", nameof(resource));
        if ((resource.Permissions & ResourcePermission.Read) == 0 ||
            configuration.MinimumWindowUs == 0 || configuration.MinimumWindowUs > configuration.MaximumWindowUs ||
            configuration.MaximumWindowUs > MaximumWindowUs || configuration.MinimumTimeoutUs == 0 ||
            configuration.MinimumTimeoutUs > configuration.MaximumTimeoutUs ||
            configuration.MaximumTimeoutUs > MaximumTimeoutUs || configuration.MaximumCount == 0 ||
            configuration.MaximumCount > MaximumCount || (configuration.SupportedEdges & ~7) != 0 ||
            configuration.SupportedEdges == 0 || (configuration.SupportedLevels & ~3) != 0 ||
            configuration.SupportedLevels == 0)
            throw new ArgumentOutOfRangeException(nameof(configuration));
        return configuration;
    }
}
