using System.Buffers.Binary;
using McuRemote;

static class BlockDeviceGoldenTests {
    private static readonly BlockDeviceResourceConfiguration Configuration =
        new(256, 1, 4, 16, 192, 192, 128, 0xff,
            BlockDeviceCapabilities.Read | BlockDeviceCapabilities.Write |
            BlockDeviceCapabilities.Erase | BlockDeviceCapabilities.Sync |
            BlockDeviceCapabilities.Persistent, 4);

    private static readonly ResourceDescriptor Resource = new(
        7, BlockDeviceJobBuilderExtensions.ModuleId,
        BlockDeviceJobBuilderExtensions.ResourceType,
        ResourcePermission.Read | ResourcePermission.Write, Configuration);

    public static void Run(Action<bool, string> check) {
        JobBuilder read = new(0x10203040, modules: new ModuleRegistry([BlockDeviceJobBuilderExtensions.Descriptor]));
        read.BlockRead(Resource, 4, 6);
        read.Return();
        check(Convert.ToHexString(read.Build()) ==
            "4A45584501022800403020103A000000120000000010270000404B4C002000800000000C000F0C00010C0007000400000006000000510100",
            "BlockDevice read golden vector");

        JobBuilder write = new(0x10203040, modules: new ModuleRegistry([BlockDeviceJobBuilderExtensions.Descriptor]));
        write.BlockWrite(Resource, 4, [0xde, 0xad, 0xbe, 0xef]);
        write.Return();
        check(Convert.ToHexString(write.Build()) ==
            "4A45584501022800403020103E000000160000000010270000404B4C002000800000000C00110C000207000400000004000000DEADBEEF510100",
            "BlockDevice write golden vector");

        JobBuilder erase = new(0x10203040, modules: new ModuleRegistry([BlockDeviceJobBuilderExtensions.Descriptor]));
        erase.BlockErase(Resource, 16, 16);
        erase.Return();
        check(Convert.ToHexString(erase.Build()) ==
            "4A45584501022800403020103A000000120000000010270000404B4C002000800000000C000F0C000307000100000010000000510100",
            "BlockDevice erase golden vector");

        JobBuilder sync = new(0x10203040, modules: new ModuleRegistry([BlockDeviceJobBuilderExtensions.Descriptor]));
        sync.BlockSync(Resource);
        sync.Return();
        check(Convert.ToHexString(sync.Build()) ==
            "4A4558450102280040302010320000000A000000000010270000404B4C002000800000000C00050C00040700510100",
            "BlockDevice sync golden vector");

        byte[] capabilities = Convert.FromHexString(
            "02022036360101000800000000010020008000000010270000404B4C0000020000160001000C0001010C00010FC001C00000103000");
        DeviceCapabilities decoded = CapabilitiesDecoder.Decode(capabilities);
        check(decoded.Modules.Count == 1 && decoded.Modules[0].ModuleId == 12 &&
              decoded.Modules[0].Payload.SequenceEqual(Convert.FromHexString("010F0401C000C00000103000")),
            "BlockDevice capability golden vector");

        Expect<ArgumentOutOfRangeException>(() => {
            JobBuilder invalid = NewBuilder();
            invalid.BlockRead(Resource, uint.MaxValue, 1);
        }, check, "BlockDevice offset overflow rejected");
        Expect<ArgumentOutOfRangeException>(() => {
            JobBuilder invalid = NewBuilder();
            invalid.BlockRead(Resource, uint.MaxValue - 10, 100);
        }, check, "BlockDevice offset plus length overflow rejected");
        Expect<ArgumentException>(() => {
            JobBuilder invalid = NewBuilder();
            invalid.BlockWrite(Resource with { Permissions = ResourcePermission.Read }, 4, [1, 2, 3, 4]);
        }, check, "BlockDevice write permission rejected");
        Expect<ArgumentOutOfRangeException>(() => {
            JobBuilder invalid = NewBuilder();
            invalid.BlockErase(Resource, 4, 16);
        }, check, "BlockDevice erase alignment rejected");
    }

    private static JobBuilder NewBuilder() =>
        new(0x10203040, modules: new ModuleRegistry([BlockDeviceJobBuilderExtensions.Descriptor]));

    private static void Expect<T>(Action action, Action<bool, string> check, string name)
        where T : Exception {
        try {
            action();
            check(false, name);
        } catch (T) {
            check(true, name);
        }
    }
}
