using System.Runtime.CompilerServices;
using McuRemote;

internal static class EncoderGoldenTests {
    [ModuleInitializer]
    internal static void Run() {
        EncoderResourceConfiguration configuration = new(25, 26, EncoderDecodeMode.X4, false, 0);
        ResourceDescriptor resource = new(10, EncoderJobBuilderExtensions.ModuleId,
            EncoderJobBuilderExtensions.ResourceType,
            ResourcePermission.Read | ResourcePermission.Write, configuration);
        JobBuilder read = new(0x10203040, modules: new ModuleRegistry([EncoderJobBuilderExtensions.Descriptor]));
        Register position = read.EncoderRead(resource);
        read.Return(position);
        string readVector = Convert.ToHexString(read.Build());
        const string expectedRead = "4A4558450102280040302010330000000B0000000200000010270000404B4C002000800000020000E0060A00010A0000510101";
        if (readVector != expectedRead) throw new Exception($"golden Encoder READ vector: {readVector}");

        JobBuilder reset = new(0x10203041, modules: new ModuleRegistry([EncoderJobBuilderExtensions.Descriptor]));
        reset.EncoderReset(resource); reset.Return();
        const string expectedReset = "4A4558450102280041302010320000000A0000000200000010270000404B4C002000800000020000E0050A00020A00510100";
        string resetVector = Convert.ToHexString(reset.Build());
        if (resetVector != expectedReset) throw new Exception($"golden Encoder RESET vector: {resetVector}");

        JobBuilder readReset = new(0x10203042, modules: new ModuleRegistry([EncoderJobBuilderExtensions.Descriptor]));
        Register previous = readReset.EncoderReadReset(resource); readReset.Return(previous);
        if (readReset.Build()[44] != EncoderJobBuilderExtensions.ReadResetOperation)
            throw new Exception("Encoder READ_RESET operation");
        if (EncoderJobBuilderExtensions.DecodeSignedPosition(0xffffffffu) != -1 ||
            EncoderJobBuilderExtensions.DecodeSignedPosition(0x80000000u) != int.MinValue ||
            EncoderJobBuilderExtensions.DecodeSignedPosition(0x7fffffffu) != int.MaxValue)
            throw new Exception("Encoder signed conversion");

        try {
            read.EncoderReset(resource with { Permissions = ResourcePermission.Read });
            throw new Exception("Encoder RESET permission accepted");
        } catch (ArgumentException) { }
        try {
            read.EncoderRead(resource with { ModuleId = 1 });
            throw new Exception("Encoder resource type accepted");
        } catch (ArgumentException) { }
    }
}
