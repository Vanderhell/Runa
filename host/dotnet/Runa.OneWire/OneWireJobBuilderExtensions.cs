using System.Buffers.Binary;

namespace McuRemote;

public sealed class OneWireRomId {
    public byte[] Bytes { get; }
    public OneWireRomId(ReadOnlySpan<byte> bytes) {
        if (bytes.Length != 8) throw new ArgumentException("A 1-Wire ROM ID must contain exactly 8 bytes.", nameof(bytes));
        Bytes = bytes.ToArray();
    }
    public bool HasValidCrc => Crc8(Bytes.AsSpan()) == Bytes[7];
    private static byte Crc8(ReadOnlySpan<byte> value) {
        byte crc = 0;
        for (int index = 0; index < 7; index++) {
            byte current = value[index];
            for (int bit = 0; bit < 8; bit++) {
                byte mix = (byte)((crc ^ current) & 1);
                crc >>= 1;
                if (mix != 0) crc ^= 0x8c;
                current >>= 1;
            }
        }
        return crc;
    }
}

public sealed record OneWireBusResourceConfiguration(
    ushort MaximumTxBytes, ushort MaximumRxBytes, ushort MaximumSearchResults,
    bool StrongPullupSupported, bool BroadcastSupported, bool ValidateRomCrc,
    uint MaximumTimeoutUs);

public sealed record OneWireDeviceResourceConfiguration(ushort BusResourceId, OneWireRomId RomId);

public static class OneWireJobBuilderExtensions {
    public const ushort ModuleId = 11;
    public const ushort BusResourceType = 1;
    public const ushort DeviceResourceType = 2;
    public const byte ResetOperation = 1;
    public const byte TransferOperation = 2;
    public const byte RomSearchOperation = 3;
    public const ushort MaximumTxBytes = 240;
    public const ushort MaximumRxBytes = 240;
    public const byte MaximumSearchResults = 16;
    public const uint MaximumTimeoutUs = 5_000_000;
    public const byte SkipRom = 1;
    public const byte StrongPullup = 2;

    public static ModuleDescriptor Descriptor { get; } =
        new(ModuleId, 1, 1, [ResetOperation, TransferOperation, RomSearchOperation]);

    public static Register OneWireReset(this JobBuilder builder, ResourceDescriptor resource,
                                        uint timeoutUs) {
        ArgumentNullException.ThrowIfNull(builder);
        CheckResource(resource, ResourcePermission.Read);
        OneWireBusResourceConfiguration configuration = GetBusConfiguration(resource);
        CheckTimeout(configuration, timeoutUs);
        Register presence = builder.Allocate();
        Span<byte> payload = stackalloc byte[7];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        BinaryPrimitives.WriteUInt32LittleEndian(payload[2..], timeoutUs);
        payload[6] = presence.Index;
        builder.EmitExtension(ModuleId, ResetOperation, payload);
        return presence;
    }

    public static void OneWireTransfer(this JobBuilder builder, ResourceDescriptor resource,
                                        ReadOnlySpan<byte> transmit, byte receiveLength,
                                        uint timeoutUs, bool skipRom = false,
                                        uint strongPullupUs = 0) {
        ArgumentNullException.ThrowIfNull(builder);
        if (resource.ResourceType != BusResourceType && resource.ResourceType != DeviceResourceType)
            throw new ArgumentException("OneWire resource type mismatch.", nameof(resource));
        OneWireBusResourceConfiguration configuration = GetBusConfiguration(resource);
        byte flags = 0;
        if (skipRom) flags |= SkipRom;
        if (strongPullupUs != 0) flags |= StrongPullup;
        if (transmit.Length == 0 && receiveLength == 0)
            throw new ArgumentOutOfRangeException(nameof(receiveLength));
        if (transmit.Length > MaximumTxBytes || receiveLength > MaximumRxBytes ||
            transmit.Length > configuration.MaximumTxBytes || receiveLength > configuration.MaximumRxBytes)
            throw new ArgumentOutOfRangeException(nameof(transmit));
        if (resource.ResourceType == BusResourceType && (!skipRom || !configuration.BroadcastSupported))
            throw new ArgumentException("Bus transfers require authorized SKIP ROM broadcast.", nameof(resource));
        if (resource.ResourceType == DeviceResourceType && skipRom)
            throw new ArgumentException("Device resources select their fixed ROM automatically.", nameof(skipRom));
        if (strongPullupUs != 0 && (!configuration.StrongPullupSupported || strongPullupUs > MaximumTimeoutUs ||
                                     (resource.Permissions & ResourcePermission.Write) == 0))
            throw new ArgumentException("Strong pull-up is not authorized.", nameof(strongPullupUs));
        if (transmit.Length != 0 && (resource.Permissions & ResourcePermission.Write) == 0)
            throw new ArgumentException("OneWire write permission is required.", nameof(resource));
        if (receiveLength != 0 && (resource.Permissions & ResourcePermission.Read) == 0)
            throw new ArgumentException("OneWire read permission is required.", nameof(resource));
        CheckTimeout(configuration, timeoutUs);
        int extra = strongPullupUs == 0 ? 0 : 4;
        Span<byte> payload = stackalloc byte[9 + extra + MaximumTxBytes];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        payload[2] = flags;
        payload[3] = checked((byte)transmit.Length);
        payload[4] = receiveLength;
        BinaryPrimitives.WriteUInt32LittleEndian(payload[5..], timeoutUs);
        int offset = 9;
        if (strongPullupUs != 0) {
            BinaryPrimitives.WriteUInt32LittleEndian(payload[offset..], strongPullupUs);
            offset += 4;
        }
        transmit.CopyTo(payload[offset..]);
        builder.EmitExtension(ModuleId, TransferOperation, payload[..(offset + transmit.Length)]);
    }

    public static void OneWireSearch(this JobBuilder builder, ResourceDescriptor resource,
                                     byte maximumResults, uint timeoutUs) {
        ArgumentNullException.ThrowIfNull(builder);
        CheckResource(resource, ResourcePermission.Read);
        if (resource.ResourceType != BusResourceType)
            throw new ArgumentException("ROM search requires a bus resource.", nameof(resource));
        OneWireBusResourceConfiguration configuration = GetBusConfiguration(resource);
        if (maximumResults is 0 or > MaximumSearchResults || maximumResults > configuration.MaximumSearchResults)
            throw new ArgumentOutOfRangeException(nameof(maximumResults));
        CheckTimeout(configuration, timeoutUs);
        Span<byte> payload = stackalloc byte[7];
        BinaryPrimitives.WriteUInt16LittleEndian(payload, resource.Id);
        payload[2] = maximumResults;
        BinaryPrimitives.WriteUInt32LittleEndian(payload[3..], timeoutUs);
        builder.EmitExtension(ModuleId, RomSearchOperation, payload);
    }

    private static void CheckResource(ResourceDescriptor resource, ResourcePermission required) {
        if (resource.ModuleId != ModuleId || (resource.ResourceType != BusResourceType &&
            resource.ResourceType != DeviceResourceType))
            throw new ArgumentException("OneWire resource type mismatch.", nameof(resource));
        if ((resource.Permissions & required) != required)
            throw new ArgumentException("OneWire resource permission mismatch.", nameof(resource));
    }
    private static OneWireBusResourceConfiguration GetBusConfiguration(ResourceDescriptor resource) {
        OneWireBusResourceConfiguration configuration;
        if (resource.Configuration is OneWireBusResourceConfiguration busConfiguration) {
            configuration = busConfiguration;
        } else if (resource.Configuration is OneWireDeviceResourceConfiguration deviceConfiguration) {
            if (deviceConfiguration.BusResourceId == 0 || !deviceConfiguration.RomId.HasValidCrc)
                throw new ArgumentException("A valid fixed ROM device configuration is required.", nameof(resource));
            configuration = new(MaximumTxBytes, MaximumRxBytes, MaximumSearchResults,
                                false, false, true, MaximumTimeoutUs);
        } else {
            throw new ArgumentException("A OneWire resource configuration is required.", nameof(resource));
        }
        if (configuration.MaximumTxBytes is 0 or > MaximumTxBytes ||
            configuration.MaximumRxBytes is 0 or > MaximumRxBytes ||
            configuration.MaximumSearchResults is 0 or > MaximumSearchResults ||
            configuration.MaximumTimeoutUs is 0 or > MaximumTimeoutUs)
            throw new ArgumentOutOfRangeException(nameof(resource));
        return configuration;
    }
    private static void CheckTimeout(OneWireBusResourceConfiguration configuration, uint timeoutUs) {
        if (timeoutUs is 0 or > MaximumTimeoutUs || timeoutUs > configuration.MaximumTimeoutUs)
            throw new ArgumentOutOfRangeException(nameof(timeoutUs));
    }
}
