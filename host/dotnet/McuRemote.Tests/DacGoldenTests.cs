using McuRemote;
using System.Runtime.CompilerServices;
internal static class DacGoldenTests {
    [ModuleInitializer] internal static void Run() {
        DacResourceConfiguration configuration = new(4095, 12);
        ResourceDescriptor resource = new(9, 9, 1, ResourcePermission.Write, configuration);
        JobBuilder builder = new(0x10203040, modules: new ModuleRegistry([DacJobBuilderExtensions.Descriptor]));
        Register value = builder.Load(2048); builder.DacWrite(resource, value); builder.Return();
        const string expected = "4A45584501022800403020103A000000120000000300000010270000404B4C00200080000002000001050000080000E006090001090000510100";
        string actual = Convert.ToHexString(builder.Build());
        if (actual != expected) throw new Exception($"golden DAC vector: {actual}");
        JobBuilder convenience = new(1, modules: new ModuleRegistry([DacJobBuilderExtensions.Descriptor]));
        convenience.DacWrite(resource, 4095u); convenience.Return();
        try { convenience.DacWrite(resource, 4096u); throw new Exception("DAC range accepted"); } catch (ArgumentOutOfRangeException) { }
        try { builder.DacWrite(resource with { Permissions = ResourcePermission.Read }, value); throw new Exception("DAC read-only accepted"); } catch (ArgumentException) { }
    }
}
