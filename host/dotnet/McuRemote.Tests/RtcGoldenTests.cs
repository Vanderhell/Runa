using McuRemote;

static class RtcGoldenTests {
    public static int Run() {
        int checks = 0;
        static void Check(ref int count, bool value, string name) { ++count; if (!value) throw new Exception(name); }
        RtcResourceConfiguration configuration = new(SetSupported: true, BatteryBacked: true, RetainedAcrossPowerLoss: true);
        ResourceDescriptor resource = new(13, 13, 1, ResourcePermission.Read | ResourcePermission.Write, configuration);
        ModuleRegistry modules = new([RtcJobBuilderExtensions.Descriptor]);
        JobBuilder read = new(0x11223344, modules: modules); read.RtcRead(resource); read.Return();
        const string expectedRead = "4A4558450102280044332211320000000A0000000200000010270000404B4C002000800000020000E0050D00010D00510100";
        Check(ref checks, Convert.ToHexString(read.Build()) == expectedRead, "RTC read vector");
        JobBuilder set = new(0x10203040, modules: modules); set.RtcSet(resource, new RunaRtcTime(2147483648)); set.Return();
        const string expectedSet = "4A45584501022800403020103A000000120000000200000010270000404B4C002000800000020000E00D0D00020D000000008000000000510100";
        Check(ref checks, Convert.ToHexString(set.Build()) == expectedSet, "RTC beyond-2038 vector");
        RunaRtcTime utc = RunaRtcTime.FromDateTimeOffset(DateTimeOffset.UnixEpoch.AddSeconds(2147483648));
        Check(ref checks, utc.Seconds == 2147483648 && utc.ToDateTimeOffset().Offset == TimeSpan.Zero, "RTC UTC conversion");
        try { RunaRtcTime.FromDateTimeOffset(new DateTimeOffset(2024, 1, 1, 0, 0, 0, TimeSpan.FromHours(1))); throw new Exception("local time accepted"); } catch (ArgumentException) { ++checks; }
        try { new JobBuilder(1, modules: modules).RtcSet(resource with { Permissions = ResourcePermission.Read }, utc); throw new Exception("write accepted"); } catch (ArgumentException) { ++checks; }
        byte[] moduleData = [4, 1, 32, 0, 0x44, 0x33, 0x22, 0x11, 13, 0, 0, 0, 0, 0, 16, 0, 1, 1, 0, 0, 0, 0, 0, 128, 0, 0, 0, 0, 25, 0, 0, 0];
        RtcTimeResult decoded = RtcModuleDataDecoder.DecodeRead(ResponseDecoder.DecodeModuleData(moduleData));
        Check(ref checks, decoded.Time.Seconds == 2147483648 && decoded.StatusFlags == (RtcStatusFlags.ValidTime | RtcStatusFlags.BatteryBacked | RtcStatusFlags.SetSupported), "RTC data decode");
        return checks;
    }
}
