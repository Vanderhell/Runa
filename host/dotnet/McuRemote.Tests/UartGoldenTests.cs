using System.Runtime.CompilerServices;
using McuRemote;

internal static class UartGoldenTests {
    [ModuleInitializer]
    internal static void Run() {
        UartResourceConfiguration configuration = new(1, 8, 1, UartParity.None,
            UartFlowControl.None, 17, 18, 115200, 64, 64, 1_000_000);
        ResourceDescriptor resource = new(8, UartJobBuilderExtensions.ModuleId,
            UartJobBuilderExtensions.ResourceType, ResourcePermission.Read | ResourcePermission.Write, configuration);
        JobBuilder builder = new(0x55667788, modules: new ModuleRegistry([UartJobBuilderExtensions.Descriptor]));
        builder.UartTransfer(resource, [0x55], 0, UartReceivePolicy.FixedLength, 0);
        builder.Return();
        const string expectedTx = "4A45584501022800887766553A000000120000000200000010270000404B4C002000800000020000E00D06000108000100000000000055510100";
        if (Convert.ToHexString(builder.Build()) != expectedTx)
            throw new Exception("golden UART TX vector");
        JobBuilder read = new(0x55667789, modules: new ModuleRegistry([UartJobBuilderExtensions.Descriptor]));
        read.UartTransfer(resource with { Permissions = ResourcePermission.Read }, [], 2,
            UartReceivePolicy.FixedLength, 100_000);
        read.Return();
        const string expectedRead = "4A455845010228008977665539000000110000000200000010270000404B4C002000800000020000E00C0600010800000200A0860100510100";
        if (Convert.ToHexString(read.Build()) != expectedRead)
            throw new Exception("golden UART RX vector");
        JobBuilder combined = new(0x5566778A, modules: new ModuleRegistry([UartJobBuilderExtensions.Descriptor]));
        combined.UartTransfer(resource, [0x55], 3, UartReceivePolicy.FixedLength, 100_000);
        combined.Return();
        const string expectedCombined = "4A455845010228008A7766553A000000120000000200000010270000404B4C002000800000020000E00D0600010800010300A086010055510100";
        string combinedActual = Convert.ToHexString(combined.Build());
        if (combinedActual != expectedCombined)
            throw new Exception($"golden UART TX/RX vector: {combinedActual}");
        try {
            builder.UartTransfer(resource with { Permissions = ResourcePermission.Read }, [0x55], 0,
                UartReceivePolicy.FixedLength, 0);
            throw new Exception("UART write permission accepted");
        } catch (ArgumentException) { }
    }
}
