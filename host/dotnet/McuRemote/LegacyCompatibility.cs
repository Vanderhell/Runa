namespace McuRemote;

public enum ResourceType : byte { Gpio = 1, Adc = 2, Pwm = 3 }

public readonly record struct Resource(
    ushort Id, ResourceType Type, ResourcePermission Permissions, uint MaximumValue = 0);

public static class LegacyModuleExtensions {
    public static Register GpioRead(this JobBuilder builder, Resource resource) =>
        GpioJobBuilderExtensions.GpioRead(builder, Descriptor(resource, ResourceType.Gpio));

    public static void GpioWrite(this JobBuilder builder, Resource resource, Register value) =>
        GpioJobBuilderExtensions.GpioWrite(builder, Descriptor(resource, ResourceType.Gpio), value);

    public static Register AdcRead(this JobBuilder builder, Resource resource) =>
        AdcJobBuilderExtensions.AdcRead(builder, Descriptor(resource, ResourceType.Adc));

    public static void PwmWrite(this JobBuilder builder, Resource resource, Register value) =>
        PwmJobBuilderExtensions.PwmWrite(builder, Descriptor(resource, ResourceType.Pwm), value);

    private static ResourceDescriptor Descriptor(Resource resource, ResourceType expected) {
        if (resource.Type != expected) throw new ArgumentException("Resource type mismatch.", nameof(resource));
        ushort module = expected switch {
            ResourceType.Gpio => GpioJobBuilderExtensions.ModuleId,
            ResourceType.Adc => AdcJobBuilderExtensions.ModuleId,
            ResourceType.Pwm => PwmJobBuilderExtensions.ModuleId,
            _ => throw new ArgumentOutOfRangeException(nameof(expected))
        };
        return new(resource.Id, module, 1, resource.Permissions, resource.MaximumValue);
    }
}
