namespace McuRemote;

public static class HardwareVectors {
    public static byte[] SimpleReturn(uint id) { var builder = new JobBuilder(id); builder.Return(); return builder.Build(); }
    public static byte[] Arithmetic(uint id) {
        var builder = new JobBuilder(id); var left = builder.Load(40); var right = builder.Load(2);
        var result = builder.Allocate(); builder.Binary(Opcode.Add, result, left, right); builder.Return(result);
        return builder.Build();
    }
    public static byte[] AdcEmit(uint id, Resource adc) {
        var builder = new JobBuilder(id); var result = builder.AdcRead(adc); builder.Emit(result); builder.Return(result);
        return builder.Build();
    }
    public static byte[] GpioWrite(uint id, Resource gpio, uint value) {
        var builder = new JobBuilder(id); var register = builder.Load(value); builder.GpioWrite(gpio, register);
        builder.Return(); return builder.Build();
    }
    public static byte[] PwmWrite(uint id, Resource pwm, uint value) {
        var builder = new JobBuilder(id); var register = builder.Load(value); builder.PwmWrite(pwm, register);
        builder.Return(); return builder.Build();
    }
    public static byte[] Delay(uint id, uint milliseconds) {
        var builder = new JobBuilder(id); builder.Delay(milliseconds); builder.Return(); return builder.Build();
    }
}
