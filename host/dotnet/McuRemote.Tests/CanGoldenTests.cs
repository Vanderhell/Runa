using System.Runtime.CompilerServices;
using McuRemote;

internal static class CanGoldenTests {
    [ModuleInitializer]
    internal static void Run() {
        CanResourceConfiguration configuration = new(0, CanFilterMode.Standard, false,
            500_000, 0x100, 0x7f0, 1_000_000);
        ResourceDescriptor resource = new(5, CanJobBuilderExtensions.ModuleId,
            CanJobBuilderExtensions.ResourceType, ResourcePermission.Read | ResourcePermission.Write, configuration);
        CanFrame frame = new(0x100, false, [0x11, 0x22]);
        JobBuilder tx = new(0x10203040, modules: new ModuleRegistry([CanJobBuilderExtensions.Descriptor]));
        tx.CanTransmit(resource, frame, 100_000); tx.Return();
        const string expectedTx = "4A45584501022800403020103E000000160000000200000010270000404B4C002000800000020000E01107000105000000010000021122A0860100510100";
        string txActual = Convert.ToHexString(tx.Build());
        if (txActual != expectedTx) throw new Exception($"golden CAN TX vector: {txActual}");
        JobBuilder extended = new(0x10203043, modules: new ModuleRegistry([CanJobBuilderExtensions.Descriptor]));
        extended.CanTransmit(resource, new CanFrame(0x1abcde, true, []), 100_000); extended.Return();
        const string expectedExtended = "4A45584501022800433020103C000000140000000200000010270000404B4C002000800000020000E00F070001050001DEBC1A0000A0860100510100";
        if (Convert.ToHexString(extended.Build()) != expectedExtended) throw new Exception("golden CAN extended TX vector");
        JobBuilder rx = new(0x10203041, modules: new ModuleRegistry([CanJobBuilderExtensions.Descriptor]));
        rx.CanReceive(resource, 100_000); rx.Return();
        const string expectedRx = "4A4558450102280041302010360000000E0000000200000010270000404B4C002000800000020000E0090700020500A0860100510100";
        if (Convert.ToHexString(rx.Build()) != expectedRx) throw new Exception("golden CAN RX vector");
        JobBuilder request = new(0x10203042, modules: new ModuleRegistry([CanJobBuilderExtensions.Descriptor]));
        request.CanRequestResponse(resource, frame, 100_000); request.Return();
        const string expectedRequest = "4A45584501022800423020103E000000160000000200000010270000404B4C002000800000020000E01107000305000000010000021122A0860100510100";
        if (Convert.ToHexString(request.Build()) != expectedRequest) throw new Exception("golden CAN request-response vector");
        ModuleDataFrame data = new(9, CanJobBuilderExtensions.ModuleId, 2, 0,
            [0, 0x23, 1, 0, 0, 2, 0xaa, 0xbb]);
        CanFrame decoded = CanModuleDataDecoder.Decode(data);
        if (decoded.Id != 0x123 || decoded.Extended || !decoded.Data.AsSpan().SequenceEqual(new byte[] { 0xaa, 0xbb }))
            throw new Exception("CAN module-data decode");
    }
}
