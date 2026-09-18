using System.Buffers.Binary;

namespace McuRemote;

public sealed class JobBuilder {
    private const int HeaderSize = 40;
    private const byte ExtensionOpcode = 0xe0;
    private readonly List<byte[]> instructions = [];
    private readonly List<ushort> jumpTargets = [];
    private readonly DeviceLimits limits;
    private readonly ModuleRegistry? modules;
    private byte nextRegister;
    private byte irVersion = 1;

    public uint JobId { get; }

    public JobBuilder(uint jobId, DeviceLimits? limits = null, ModuleRegistry? modules = null) {
        JobId = jobId;
        this.limits = limits ?? DeviceLimits.Default;
        this.modules = modules;
        ValidateLimits(this.limits);
    }

    private static void ValidateLimits(DeviceLimits value) {
        if (value.MaxJobBytes > 2048 || value.MaxInstructions > 256 || value.MaxSteps is 0 or > 10000 ||
            value.MaxRuntimeUs is 0 or > 5_000_000 || value.MaxResultBytes > 32 ||
            value.MaxEmits > 128 || value.MaxEmitBytes > 512)
            throw new ArgumentOutOfRangeException(nameof(value));
    }

    public Register Allocate() {
        if (nextRegister >= 8) throw new InvalidOperationException("No registers available.");
        return new(nextRegister++);
    }

    private void Add(byte opcode, ReadOnlySpan<byte> operands) {
        if (instructions.Count >= limits.MaxInstructions) throw new InvalidOperationException("Instruction limit exceeded.");
        if (operands.Length > byte.MaxValue) throw new ArgumentOutOfRangeException(nameof(operands));
        byte[] instruction = new byte[2 + operands.Length];
        instruction[0] = opcode;
        instruction[1] = checked((byte)operands.Length);
        operands.CopyTo(instruction.AsSpan(2));
        instructions.Add(instruction);
    }

    private void Add(Opcode opcode, params byte[] operands) => Add((byte)opcode, operands);

    private static byte[] U16(ushort value) {
        byte[] bytes = new byte[2];
        BinaryPrimitives.WriteUInt16LittleEndian(bytes, value);
        return bytes;
    }

    private static byte[] U32(uint value) {
        byte[] bytes = new byte[4];
        BinaryPrimitives.WriteUInt32LittleEndian(bytes, value);
        return bytes;
    }

    public void Nop() => Add(Opcode.Nop);
    public Register Load(uint value) {
        Register register = Allocate();
        Add(Opcode.LoadConst, [register.Index, .. U32(value)]);
        return register;
    }
    public void Move(Register destination, Register source) => Add(Opcode.Mov, destination.Index, source.Index);
    public void Binary(Opcode opcode, Register destination, Register left, Register right) {
        if (opcode is < Opcode.Add or > Opcode.CmpGe || opcode == Opcode.Not)
            throw new ArgumentException("Not a binary opcode.", nameof(opcode));
        Add(opcode, destination.Index, left.Index, right.Index);
    }
    public void Not(Register destination, Register source) => Add(Opcode.Not, destination.Index, source.Index);
    public ushort Position => checked((ushort)instructions.Count);
    public void Jump(ushort target) { jumpTargets.Add(target); Add(Opcode.Jump, U16(target)); }
    public void JumpIf(Register condition, ushort target, bool whenTrue = true) {
        jumpTargets.Add(target);
        Add(whenTrue ? Opcode.JumpIf : Opcode.JumpIfNot, [condition.Index, .. U16(target)]);
    }

    public void EmitExtension(ushort moduleId, byte operation, ReadOnlySpan<byte> payload) {
        if (moduleId == 0) throw new ArgumentOutOfRangeException(nameof(moduleId));
        if (modules is not null && !modules.Supports(moduleId, operation))
            throw new ArgumentException("Module operation is not registered.", nameof(operation));
        if (payload.Length > byte.MaxValue - 3) throw new ArgumentOutOfRangeException(nameof(payload));
        byte[] operands = new byte[3 + payload.Length];
        BinaryPrimitives.WriteUInt16LittleEndian(operands, moduleId);
        operands[2] = operation;
        payload.CopyTo(operands.AsSpan(3));
        Add(ExtensionOpcode, operands);
        irVersion = 2;
    }

    public void Delay(uint milliseconds) {
        if (milliseconds > 1000) throw new ArgumentOutOfRangeException(nameof(milliseconds));
        Add(Opcode.DelayMs, U32(milliseconds));
    }
    public void Emit(Register value) => Add(Opcode.Emit, value.Index);
    public void Return(params Register[] values) {
        byte mask = 0;
        foreach (Register register in values) mask |= (byte)(1 << register.Index);
        if (values.Length * 4 > limits.MaxResultBytes) throw new InvalidOperationException("Result limit exceeded.");
        Add(Opcode.Return, mask);
    }

    public byte[] Build() {
        if (instructions.Count == 0) throw new InvalidOperationException("Empty job.");
        if (jumpTargets.Any(target => target >= instructions.Count)) throw new InvalidOperationException("Invalid branch target.");
        int instructionBytes = instructions.Sum(instruction => instruction.Length);
        int total = HeaderSize + instructionBytes;
        if (total > limits.MaxJobBytes) throw new InvalidOperationException("Job size exceeded.");
        byte[] output = new byte[total];
        "JEXE"u8.CopyTo(output);
        output[4] = 1;
        output[5] = irVersion;
        BinaryPrimitives.WriteUInt16LittleEndian(output.AsSpan(6), HeaderSize);
        BinaryPrimitives.WriteUInt32LittleEndian(output.AsSpan(8), JobId);
        BinaryPrimitives.WriteUInt32LittleEndian(output.AsSpan(12), (uint)total);
        BinaryPrimitives.WriteUInt32LittleEndian(output.AsSpan(16), (uint)instructionBytes);
        BinaryPrimitives.WriteUInt16LittleEndian(output.AsSpan(20), (ushort)instructions.Count);
        BinaryPrimitives.WriteUInt32LittleEndian(output.AsSpan(24), limits.MaxSteps);
        BinaryPrimitives.WriteUInt32LittleEndian(output.AsSpan(28), limits.MaxRuntimeUs);
        BinaryPrimitives.WriteUInt16LittleEndian(output.AsSpan(32), limits.MaxResultBytes);
        BinaryPrimitives.WriteUInt16LittleEndian(output.AsSpan(34), limits.MaxEmits);
        BinaryPrimitives.WriteUInt32LittleEndian(output.AsSpan(36), limits.MaxEmitBytes);
        int offset = HeaderSize;
        foreach (byte[] instruction in instructions) {
            instruction.CopyTo(output, offset);
            offset += instruction.Length;
        }
        return output;
    }
}
