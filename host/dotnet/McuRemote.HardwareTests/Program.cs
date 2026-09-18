using System.Buffers.Binary;
using McuRemote;
using Runa.Transport.Serial;

static class HardwareTests {
    static readonly Resource GpioOut = new(1, ResourceType.Gpio, ResourcePermission.Write, 1);
    static readonly Resource GpioIn = new(2, ResourceType.Gpio, ResourcePermission.Read, 1);
    static readonly Resource Adc = new(3, ResourceType.Adc, ResourcePermission.Read, 4095);
    static readonly Resource Pwm = new(4, ResourceType.Pwm, ResourcePermission.Write, 10000);
    static int checks;

    static void Check(bool condition, string name) {
        checks++;
        if (!condition) throw new Exception(name);
    }

    static byte[] Raw(uint id, byte[] instructions, ushort count, uint steps = 10000, uint runtimeUs = 5_000_000) {
        byte[] bytes = new byte[40 + instructions.Length];
        "JEXE"u8.CopyTo(bytes);
        bytes[4] = 1;
        bytes[5] = 1;
        BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(6), 40);
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(8), id);
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(12), (uint)bytes.Length);
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(16), (uint)instructions.Length);
        BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(20), count);
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(24), steps);
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(28), runtimeUs);
        BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(32), 32);
        BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(34), 128);
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(36), 512);
        instructions.CopyTo(bytes, 40);
        return bytes;
    }

    static byte[] Arithmetic(uint id) {
        var job = new JobBuilder(id);
        var a = job.Load(40);
        var b = job.Load(2);
        var result = job.Allocate();
        job.Binary(Opcode.Add, result, a, b);
        job.Return(result);
        return job.Build();
    }

    static byte[] Branch(uint id) {
        var job = new JobBuilder(id);
        var a = job.Load(5);
        var b = job.Load(4);
        var condition = job.Allocate();
        job.Binary(Opcode.CmpGt, condition, a, b);
        job.JumpIf(condition, 5);
        job.Load(0);
        job.Return(condition);
        return job.Build();
    }

    static byte[] Emit(uint id) {
        var job = new JobBuilder(id);
        var value = job.Load(42);
        job.Emit(value);
        job.Return(value);
        return job.Build();
    }

    static byte[] Delay(uint id) {
        var job = new JobBuilder(id);
        job.Delay(5);
        job.Return();
        return job.Build();
    }

    static byte[] Combined(uint id) {
        var job = new JobBuilder(id);
        var high = job.Load(1);
        var half = job.Load(5000);
        var zero = job.Load(0);
        job.GpioWrite(GpioOut, high);
        job.Delay(20);
        var input = job.GpioRead(GpioIn);
        var sample = job.AdcRead(Adc);
        job.PwmWrite(Pwm, half);
        job.Emit(input);
        job.Emit(sample);
        job.Delay(100);
        job.PwmWrite(Pwm, zero);
        job.GpioWrite(GpioOut, zero);
        job.Return(input, sample);
        return job.Build();
    }

    static byte[] Loopback(uint id, uint level) {
        var job = new JobBuilder(id);
        var output = job.Load(level);
        job.GpioWrite(GpioOut, output);
        job.Delay(10);
        var input = job.GpioRead(GpioIn);
        job.Return(input);
        return job.Build();
    }

    static async Task VerifyLoopback(HardwareTestRunner runner, uint id, uint level) {
        var result = await Execute(runner, Loopback(id, level), 2, 0);
        Check(result.Values is [var actual] && actual == level, $"GPIO loopback expected {level}");
    }

    static async Task RunGpioLoopback(HardwareTestRunner runner) {
        await VerifyLoopback(runner, 10_000, 0);
        await VerifyLoopback(runner, 10_001, 1);

        for (uint i = 0; i < 5_000; i++)
            await VerifyLoopback(runner, 11_000 + i, i & 1u);

        await Execute(runner, Raw(20_000, [0x30, 3, 1, 0, 0], 1), 1, 12);
        await VerifyLoopback(runner, 20_001, 0);
        await VerifyLoopback(runner, 20_002, 1);
        await Execute(runner, Raw(20_003, [0x31, 3, 2, 0, 0], 1), 1, 12);
        await VerifyLoopback(runner, 20_004, 0);
        await VerifyLoopback(runner, 20_005, 1);
        await Execute(runner, Raw(20_006, [0x30, 3, 99, 0, 0], 1), 1, 10);
        await VerifyLoopback(runner, 20_007, 0);
        await Execute(runner, Raw(20_008, [0x32, 3, 2, 0, 0], 1), 1, 11);
        await VerifyLoopback(runner, 20_009, 1);
        await VerifyLoopback(runner, 20_010, 0);

        Console.WriteLine($"{checks} GPIO electrical checks passed; transitions=5000; mismatches=0");
    }

    static async Task RunAdcSamples(HardwareTestRunner runner, string state) {
        const int sampleCount = 100;
        uint minimum = uint.MaxValue;
        uint maximum = 0;
        ulong total = 0;
        for (uint i = 0; i < sampleCount; i++) {
            var result = await Execute(runner, HardwareVectors.AdcEmit(30_000 + i, Adc), 3, 0);
            Check(result.Values is [<= 4095], "ADC electrical sample range");
            uint value = result.Values![0];
            minimum = Math.Min(minimum, value);
            maximum = Math.Max(maximum, value);
            total += value;
        }
        Console.WriteLine($"ADC {state}: samples={sampleCount}; min={minimum}; max={maximum}; mean={(double)total / sampleCount:F2}; checks={checks}");
    }

    static byte[] PwmObserve(uint id, uint duty) {
        var job = new JobBuilder(id);
        var value = job.Load(duty);
        job.PwmWrite(Pwm, value);
        job.Delay(2);
        var input = job.GpioRead(GpioIn);
        job.Return(input);
        return job.Build();
    }

    static async Task RunPwmDigital(HardwareTestRunner runner) {
        const int transitions = 1_000;
        for (uint i = 0; i < transitions; i++) {
            uint duty = (i & 1u) == 0 ? 0u : 10_000u;
            uint expected = duty == 0 ? 0u : 1u;
            var result = await Execute(runner, PwmObserve(40_000 + i, duty), 2, 0);
            Check(result.Values is [var actual] && actual == expected, $"PWM boundary expected {expected}");
        }
        foreach (uint duty in new uint[] { 2500, 5000, 7500 })
            await Execute(runner, HardwareVectors.PwmWrite(42_000 + duty, Pwm, duty), 2, 0);
        await Execute(runner, HardwareVectors.PwmWrite(50_000, Pwm, 0), 2, 0);
        Console.WriteLine($"{checks} PWM electrical checks passed; boundary transitions={transitions}; mismatches=0; intermediate values accepted but not measured");
    }

    static byte[] ObserveInput(uint id) {
        var job = new JobBuilder(id);
        job.Delay(2);
        var input = job.GpioRead(GpioIn);
        job.Return(input);
        return job.Build();
    }

    static byte[] RuntimeFailureAfterGpioHigh(uint id) {
        var job = new JobBuilder(id);
        var high = job.Load(1);
        job.GpioWrite(GpioOut, high);
        var invalidDuty = job.Load(10_001);
        job.PwmWrite(Pwm, invalidDuty);
        var low = job.Load(0);
        job.GpioWrite(GpioOut, low);
        job.Return();
        return job.Build();
    }

    static async Task RunCombinedAndMixed(HardwareTestRunner runner) {
        var combined = await Execute(runner, Combined(60_000), 4, 0);
        Check(combined.Values is [1, <= 4095], "combined GPIO and ADC values");

        byte[] validationFailure = Raw(60_001,
            [0x01, 5, 0, 1, 0, 0, 0, 0x31, 3, 1, 0, 0, 0x32, 3, 99, 0, 1], 3);
        var validationResult = await Execute(runner, validationFailure, 1, 10);
        Check(validationResult.Instruction == 2, "validation failure instruction");
        await VerifyLoopback(runner, 60_002, 0);

        var runtimeResult = await Execute(runner, RuntimeFailureAfterGpioHigh(60_003), 2, 13);
        Check(runtimeResult.Instruction == 3, "runtime failure instruction");
        var observedHigh = await Execute(runner, ObserveInput(60_004), 2, 0);
        Check(observedHigh.Values is [1], "runtime partial execution left GPIO high");
        await VerifyLoopback(runner, 60_005, 0);

        int jobs = 0;
        for (int i = 0; i < 100; i++) {
            uint id = (uint)(70_000 + i * 10);
            await VerifyLoopback(runner, id, (uint)(i & 1));
            await Execute(runner, Arithmetic(id + 1), 2, 0);
            await Execute(runner, Branch(id + 2), 2, 0);
            await Execute(runner, HardwareVectors.AdcEmit(id + 3, Adc), 3, 0);
            await Execute(runner, HardwareVectors.PwmWrite(id + 4, Pwm, (uint)((i & 1) * 10_000)), 2, 0);
            await Execute(runner, Delay(id + 5), 2, 0);
            await Execute(runner, Emit(id + 6), 3, 0);
            await Execute(runner, Combined(id + 7), 4, 0);
            await Execute(runner, Raw(id + 8, [0x30, 3, 1, 0, 0], 1), 1, 12);
            await Execute(runner, Raw(id + 9, [0x32, 3, 99, 0, 0], 1), 1, 10);
            jobs += 10;
        }
        await VerifyLoopback(runner, 80_000, 0);
        await Execute(runner, HardwareVectors.PwmWrite(80_001, Pwm, 0), 2, 0);
        Console.WriteLine($"{checks} combined/recovery checks passed; mixed jobs={jobs}; GPIO mismatches=0");
    }

    static async Task<Response> Execute(HardwareTestRunner runner, byte[] job, int events, byte error) {
        var trace = await runner.ExecuteAsync(job);
        Check(trace.Events.Count == events, "event count");
        Check(trace.Result.Error == error, $"error {error}");
        return trace.Result;
    }

    static async Task Main(string[] args) {
        string port = args.Length == 0 ? "COM37" : args[0];
        string mode = args.Length < 2 ? "baseline" : args[1];
        await using var transport = new WindowsSerialHardwareTransport(port, TimeSpan.FromSeconds(5));
        var runner = new HardwareTestRunner(transport);

        if (mode == "gpio-loopback") {
            await RunGpioLoopback(runner);
            return;
        }
        if (mode is "adc-low" or "adc-high") {
            await RunAdcSamples(runner, mode == "adc-low" ? "LOW" : "HIGH");
            return;
        }
        if (mode == "pwm-digital") {
            await RunPwmDigital(runner);
            return;
        }
        if (mode == "combined-mixed") {
            await RunCombinedAndMixed(runner);
            return;
        }

        byte[] info = await runner.GetInfoAsync();
        Check(info.Length == 30 && info[0] == 0x10, "GET_INFO frame");
        Check(info[8] == 1 && info[9] == 1 && info[13] == 1, "versions/platform");
        Check(BinaryPrimitives.ReadUInt16LittleEndian(info.AsSpan(14)) == 2048, "job size");
        Check(info[23] == 4, "resource count");
        Check(BinaryPrimitives.ReadUInt32LittleEndian(info.AsSpan(24)) == 16u * 1024u * 1024u, "flash size");
        Check(BinaryPrimitives.ReadUInt16LittleEndian(info.AsSpan(28)) == 8, "PSRAM size");

        byte[] caps = await runner.GetCapabilitiesAsync();
        Check(caps.Length == 41 && caps[0] == 0x11, "GET_CAPABILITIES frame");
        Check(caps[9] == 4, "physical resource table");
        DeviceCapabilities modularCapabilities = await runner.GetCapabilitiesV2Async();
        Check(modularCapabilities.IrVersion == 2 && modularCapabilities.Modules.Count == 3,
            "modular capabilities module count");
        Check(modularCapabilities.Modules.Select(x => x.ModuleId).Order().SequenceEqual(new ushort[] { 1, 2, 3 }),
            "modular capabilities module IDs");

        await Execute(runner, HardwareVectors.SimpleReturn(1), 2, 0);
        var arithmetic = await Execute(runner, Arithmetic(2), 2, 0);
        Check(arithmetic.Values is [42], "arithmetic value");
        var branch = await Execute(runner, Branch(3), 2, 0);
        Check(branch.Values is [1], "branch value");
        await Execute(runner, Delay(4), 2, 0);
        var emitted = await runner.ExecuteAsync(Emit(5));
        Check(emitted.Events.Count == 3 && emitted.Events[1].Type == 2 && emitted.Events[1].Values is [42], "EMIT sequence");

        await Execute(runner, HardwareVectors.GpioWrite(20, GpioOut, 1), 2, 0);
        await Execute(runner, HardwareVectors.Delay(21, 250), 2, 0);
        await Execute(runner, HardwareVectors.GpioWrite(22, GpioOut, 0), 2, 0);
        foreach (uint duty in new uint[] { 0, 2500, 5000, 7500, 10000 })
            await Execute(runner, HardwareVectors.PwmWrite(30 + duty, Pwm, duty), 2, 0);

        uint adcMin = uint.MaxValue;
        uint adcMax = 0;
        for (uint i = 0; i < 16; i++) {
            var sample = await Execute(runner, HardwareVectors.AdcEmit(40 + i, Adc), 3, 0);
            Check(sample.Values is [<= 4095], "ADC range");
            adcMin = Math.Min(adcMin, sample.Values![0]);
            adcMax = Math.Max(adcMax, sample.Values[0]);
        }
        var combined = await Execute(runner, Combined(60), 4, 0);
        Check(combined.Values is [var gpio, <= 4095] && gpio <= 1, "combined ranges");

        await Execute(runner, Raw(70, [0x31, 3, 3, 0, 0], 1), 1, 11);
        await Execute(runner, Raw(71, [0x30, 3, 1, 0, 0], 1), 1, 12);
        await Execute(runner, HardwareVectors.PwmWrite(72, Pwm, 10001), 2, 13);
        await Execute(runner, Raw(73, [0x32, 3, 99, 0, 0, 0x51, 1, 0], 2), 1, 10);
        await Execute(runner, Raw(74, [0x32, 3, 1, 0, 0], 1), 1, 11);
        await Execute(runner, Raw(75, [0x99, 0], 1), 1, 6);
        await Execute(runner, Raw(76, [0x50, 1, 8], 1), 1, 8);
        await Execute(runner, Raw(77, [0x20, 2, 4, 0], 1), 1, 9);
        byte[] version = HardwareVectors.SimpleReturn(78);
        version[5] = 2;
        await Execute(runner, version, 1, 4);
        await Execute(runner, Raw(79, [0x20, 2, 0, 0], 1, 5), 2, 14);
        await Execute(runner, Raw(80, [0x40, 4, 10, 0, 0, 0, 0x51, 1, 0], 2, 100, 1000), 2, 15);
        byte[] malformed = HardwareVectors.SimpleReturn(81)[..^1];
        await Execute(runner, malformed, 1, 1);
        await transport.SendMalformedAsync(new byte[] {(byte)'X', (byte)'X', (byte)'X', (byte)'X', 1, 3, 0xff, 0xff});
        info = await runner.GetInfoAsync();
        Check(info[0] == 0x10, "framing recovery");

        int stressJobs = 0;
        for (int i = 0; i < 100; i++) {
            uint id = (uint)(1000 + i * 10);
            await Execute(runner, HardwareVectors.SimpleReturn(id), 2, 0);
            await Execute(runner, Arithmetic(id + 1), 2, 0);
            await Execute(runner, Branch(id + 2), 2, 0);
            await Execute(runner, HardwareVectors.GpioWrite(id + 3, GpioOut, (uint)(i & 1)), 2, 0);
            await Execute(runner, HardwareVectors.AdcEmit(id + 4, Adc), 3, 0);
            await Execute(runner, HardwareVectors.PwmWrite(id + 5, Pwm, (uint)((i % 5) * 2500)), 2, 0);
            await Execute(runner, Delay(id + 6), 2, 0);
            await Execute(runner, Emit(id + 7), 3, 0);
            await Execute(runner, Raw(id + 8, [0x32, 3, 99, 0, 0, 0x51, 1, 0], 2), 1, 10);
            if ((i & 1) == 0)
                await Execute(runner, Raw(id + 9, [0x30, 3, 1, 0, 0], 1), 1, 12);
            else
                await Execute(runner, Raw(id + 9, [0x20, 2, 0, 0], 1, 5), 2, 14);
            stressJobs += 10;
        }
        await Execute(runner, HardwareVectors.GpioWrite(3000, GpioOut, 0), 2, 0);
        await Execute(runner, HardwareVectors.PwmWrite(3001, Pwm, 0), 2, 0);
        Console.WriteLine($"{checks} hardware checks passed on {port}; stress jobs={stressJobs}; ADC raw range={adcMin}..{adcMax}");
    }
}
