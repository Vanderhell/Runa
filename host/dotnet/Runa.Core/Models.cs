namespace McuRemote;

public enum Opcode : byte {
    Nop = 0x00, LoadConst = 0x01, Mov = 0x02, Add = 0x03, Sub = 0x04,
    And = 0x05, Or = 0x06, Xor = 0x07, Not = 0x08, Shl = 0x09, Shr = 0x0a,
    CmpEq = 0x10, CmpNe = 0x11, CmpLt = 0x12, CmpLe = 0x13, CmpGt = 0x14, CmpGe = 0x15,
    Jump = 0x20, JumpIf = 0x21, JumpIfNot = 0x22, DelayMs = 0x40, Emit = 0x50, Return = 0x51
}

[Flags]
public enum ResourcePermission : uint { Read = 1, Write = 2 }

public readonly record struct Register {
    public byte Index { get; }
    public Register(byte index) {
        if (index >= 8) throw new ArgumentOutOfRangeException(nameof(index));
        Index = index;
    }
}

public readonly record struct ResourceDescriptor(
    ushort Id, ushort ModuleId, ushort ResourceType, ResourcePermission Permissions,
    object? Configuration = null);

public readonly record struct DeviceLimits(
    uint MaxJobBytes = 2048, ushort MaxInstructions = 256, uint MaxSteps = 10000,
    uint MaxRuntimeUs = 5_000_000, ushort MaxResultBytes = 32, ushort MaxEmits = 128,
    uint MaxEmitBytes = 512) {
    public static DeviceLimits Default => new(2048, 256, 10000, 5_000_000, 32, 128, 512);
}
