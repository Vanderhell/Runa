using System.Buffers.Binary;

namespace McuRemote;

public sealed record Response(byte Type, uint JobId, byte Status, byte Error,
                              ushort Instruction, uint Detail, uint[] Values) {
    public ushort ModuleId { get; init; }
    public ushort Sequence { get; init; }
    public byte[] Data { get; init; } = [];
}
public sealed record ModuleDataFrame(uint JobId, ushort ModuleId, ushort Instruction,
                                     ushort Sequence, byte[] Data);

public static class ResponseDecoder {
    public static ModuleDataFrame DecodeModuleData(ReadOnlySpan<byte> bytes) {
        if (bytes.Length < 16 || bytes.Length > 64 || bytes[0] != 4 || bytes[1] != 1 ||
            BinaryPrimitives.ReadUInt16LittleEndian(bytes[2..]) != bytes.Length)
            throw new FormatException("Invalid module data frame.");
        ushort dataLength = BinaryPrimitives.ReadUInt16LittleEndian(bytes[14..]);
        ushort moduleId = BinaryPrimitives.ReadUInt16LittleEndian(bytes[8..]);
        if (moduleId == 0 || dataLength > 48 || 16 + dataLength != bytes.Length)
            throw new FormatException("Invalid module data payload.");
        return new(BinaryPrimitives.ReadUInt32LittleEndian(bytes[4..]), moduleId,
            BinaryPrimitives.ReadUInt16LittleEndian(bytes[10..]),
            BinaryPrimitives.ReadUInt16LittleEndian(bytes[12..]), bytes.Slice(16, dataLength).ToArray());
    }

    public static Response Decode(ReadOnlySpan<byte> bytes) {
        if (bytes.Length < 8 || bytes[1] != 1 ||
            BinaryPrimitives.ReadUInt16LittleEndian(bytes[2..]) != bytes.Length)
            throw new FormatException("Invalid frame.");
        byte type = bytes[0];
        uint id = BinaryPrimitives.ReadUInt32LittleEndian(bytes[4..]);
        if (type == 1 && bytes.Length == 8) return new(type, id, 0, 0, 0, 0, []);
        if (type == 4) {
            ModuleDataFrame data = DecodeModuleData(bytes);
            return new(type, data.JobId, 0, 0, data.Instruction, 0, []) {
                ModuleId = data.ModuleId, Sequence = data.Sequence, Data = data.Data
            };
        }
        if (type == 2 && bytes.Length == 14)
            return new(type, id, 0, 0, BinaryPrimitives.ReadUInt16LittleEndian(bytes[8..]), 0,
                [BinaryPrimitives.ReadUInt32LittleEndian(bytes[10..])]);
        if (type != 3 || bytes.Length < 20) throw new FormatException("Invalid frame type or length.");
        ushort payloadLength = BinaryPrimitives.ReadUInt16LittleEndian(bytes[16..]);
        if (payloadLength % 4 != 0 || 20 + payloadLength != bytes.Length)
            throw new FormatException("Invalid payload.");
        uint[] values = new uint[payloadLength / 4];
        for (int index = 0; index < values.Length; index++)
            values[index] = BinaryPrimitives.ReadUInt32LittleEndian(bytes[(20 + index * 4)..]);
        return new(type, id, bytes[8], bytes[9], BinaryPrimitives.ReadUInt16LittleEndian(bytes[10..]),
            BinaryPrimitives.ReadUInt32LittleEndian(bytes[12..]), values);
    }

    public static string ErrorMessage(byte code, ushort instruction, uint detail) => code switch {
        0 => "Success.",
        8 => $"Invalid register at instruction {instruction}.",
        10 => $"Resource {detail} does not exist at instruction {instruction}.",
        12 => $"Access denied at instruction {instruction}.",
        14 => $"Execution step limit exceeded at instruction {instruction}.",
        15 => $"Runtime limit exceeded at instruction {instruction}.",
        _ => $"Device error {code} at instruction {instruction} (detail {detail})."
    };
}
