using System.Runtime.CompilerServices;
using McuRemote;

internal static class WatchdogGoldenTests {
    [ModuleInitializer]
    internal static void Run() {
        WatchdogResourceConfiguration configuration = new(100, 10_000, 1_000,
            StatusSupported: true, ArmSupported: true, FeedSupported: true,
            DisarmSupported: true);
        ResourceDescriptor resource = new(14, WatchdogJobBuilderExtensions.ModuleId,
            WatchdogJobBuilderExtensions.ResourceType,
            ResourcePermission.Read | ResourcePermission.Write, configuration);
        ModuleRegistry registry = new([WatchdogJobBuilderExtensions.Descriptor]);

        JobBuilder status = new(0x10203040, modules: registry);
        status.WatchdogGetStatus(resource); status.Return();
        const string expectedStatus = "4A4558450102280040302010320000000A0000000200000010270000404B4C002000800000020000E0050E00010E00510100";
        if (Convert.ToHexString(status.Build()) != expectedStatus) throw new Exception("Watchdog STATUS golden vector");

        JobBuilder arm = new(0x10203041, modules: registry);
        arm.WatchdogArm(resource, 1_000); arm.Return();
        const string expectedArm = "4A4558450102280041302010360000000E0000000200000010270000404B4C002000800000020000E0090E00020E00E8030000510100";
        if (Convert.ToHexString(arm.Build()) != expectedArm) throw new Exception("Watchdog ARM golden vector");

        JobBuilder feed = new(0x10203042, modules: registry);
        feed.WatchdogFeed(resource); feed.Return();
        const string expectedFeed = "4A4558450102280042302010320000000A0000000200000010270000404B4C002000800000020000E0050E00030E00510100";
        if (Convert.ToHexString(feed.Build()) != expectedFeed) throw new Exception("Watchdog FEED golden vector");

        JobBuilder disarm = new(0x10203043, modules: registry);
        disarm.WatchdogDisarm(resource); disarm.Return();
        const string expectedDisarm = "4A4558450102280043302010320000000A0000000200000010270000404B4C002000800000020000E0050E00040E00510100";
        if (Convert.ToHexString(disarm.Build()) != expectedDisarm) throw new Exception("Watchdog DISARM golden vector");

        WatchdogStatus decoded = WatchdogJobBuilderExtensions.DecodeStatus([1, 0x27, 0, 0, 0xe8, 3, 0, 0]);
        if (!decoded.Armed || !decoded.DisarmSupported || decoded.TimeoutMs != 1_000)
            throw new Exception("Watchdog status decode");
        try { arm.WatchdogArm(resource with { Permissions = ResourcePermission.Read }, 1_000); throw new Exception("Watchdog WRITE permission accepted"); }
        catch (ArgumentException) { }
        try { arm.WatchdogArm(resource, 99); throw new Exception("Watchdog minimum timeout accepted"); }
        catch (ArgumentOutOfRangeException) { }
        try { arm.WatchdogDisarm(resource with { Configuration = configuration with { DisarmSupported = false } }); throw new Exception("Watchdog unsupported DISARM accepted"); }
        catch (ArgumentException) { }
    }
}
