using System.Buffers.Binary;

namespace McuRemote;

public sealed record ModuleCapability(
    ushort ModuleId, byte AbiVersion, byte ModuleVersion, byte[] Payload);

public sealed record DeviceCapabilities(
    byte CapabilityVersion, byte ProtocolVersion, byte IrVersion, byte RegisterCount,
    uint MaxJobBytes, ushort MaxInstructions, ushort MaxResultBytes, ushort MaxEmits,
    uint MaxSteps, uint MaxRuntimeUs, uint MaxEmitBytes,
    byte ModuleRecordCount, IReadOnlyList<ModuleCapability> Modules);

public static class CapabilitiesDecoder {
    private const int HeaderSize = 32;
    private const int MaximumBytes = 2048;
    private const byte ModuleRecordType = 1;

    public static DeviceCapabilities Decode(ReadOnlySpan<byte> bytes) {
        if (bytes.Length < HeaderSize || bytes.Length > MaximumBytes || bytes[0] != 2 ||
            bytes[3] < HeaderSize || bytes[3] > bytes.Length ||
            BinaryPrimitives.ReadUInt16LittleEndian(bytes[4..]) != bytes.Length || bytes[7] != 8)
            throw new FormatException("Invalid capabilities header.");

        byte recordCount = bytes[6];
        List<ModuleCapability> modules = new();
        HashSet<ushort> ids = new();
        int offset = bytes[3];
        for (int record = 0; record < recordCount; record++) {
            if (offset > bytes.Length - 2) throw new FormatException("Truncated capability record.");
            ushort length = BinaryPrimitives.ReadUInt16LittleEndian(bytes[offset..]);
            if (length < 4 || length > bytes.Length - offset) throw new FormatException("Invalid capability record length.");
            if (bytes[offset + 2] == ModuleRecordType) {
                if (length < 10) throw new FormatException("Truncated module capability.");
                ushort payloadLength = BinaryPrimitives.ReadUInt16LittleEndian(bytes[(offset + 8)..]);
                if (payloadLength > length - 10) throw new FormatException("Invalid module capability payload length.");
                ushort moduleId = BinaryPrimitives.ReadUInt16LittleEndian(bytes[(offset + 4)..]);
                if (moduleId == 0 || !ids.Add(moduleId)) throw new FormatException("Invalid or duplicate module ID.");
                modules.Add(new(moduleId, bytes[offset + 6], bytes[offset + 7],
                    bytes.Slice(offset + 10, payloadLength).ToArray()));
            }
            offset += length;
        }
        if (offset != bytes.Length) throw new FormatException("Trailing capability data.");

        return new(bytes[0], bytes[1], bytes[2], bytes[7],
            BinaryPrimitives.ReadUInt32LittleEndian(bytes[8..]),
            BinaryPrimitives.ReadUInt16LittleEndian(bytes[12..]),
            BinaryPrimitives.ReadUInt16LittleEndian(bytes[14..]),
            BinaryPrimitives.ReadUInt16LittleEndian(bytes[16..]),
            BinaryPrimitives.ReadUInt32LittleEndian(bytes[20..]),
            BinaryPrimitives.ReadUInt32LittleEndian(bytes[24..]),
            BinaryPrimitives.ReadUInt32LittleEndian(bytes[28..]), recordCount, modules);
    }
}
