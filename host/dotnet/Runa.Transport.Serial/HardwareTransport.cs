using McuRemote;

namespace Runa.Transport.Serial;

public enum DeviceCommand : byte { GetInfo = 1, GetCapabilities = 2, Execute = 3, GetCapabilitiesV2 = 4 }

public interface IHardwareTransport : IAsyncDisposable {
    IAsyncEnumerable<ReadOnlyMemory<byte>> ExchangeAsync(
        DeviceCommand command, ReadOnlyMemory<byte> payload, CancellationToken cancellationToken);
}

public sealed record ExecutionTrace(IReadOnlyList<Response> Events) {
    public Response Result => Events.Single(value => value.Type == 3);
}

public sealed class HardwareTestRunner(IHardwareTransport transport) {
    public async Task<byte[]> GetInfoAsync(CancellationToken token = default) =>
        await Query(DeviceCommand.GetInfo, token);

    public async Task<byte[]> GetCapabilitiesAsync(CancellationToken token = default) =>
        await Query(DeviceCommand.GetCapabilities, token);

    public async Task<DeviceCapabilities> GetCapabilitiesV2Async(CancellationToken token = default) {
        byte[] frame = await Query(DeviceCommand.GetCapabilitiesV2, token);
        if (frame.Length < 8 || frame[0] != 0x12) throw new FormatException("Invalid capabilities V2 frame.");
        return CapabilitiesDecoder.Decode(frame.AsSpan(8));
    }

    private async Task<byte[]> Query(DeviceCommand command, CancellationToken token) {
        await foreach (ReadOnlyMemory<byte> frame in transport.ExchangeAsync(
                           command, ReadOnlyMemory<byte>.Empty, token)) return frame.ToArray();
        throw new IOException("Device returned no response.");
    }

    public async Task<ExecutionTrace> ExecuteAsync(byte[] job, CancellationToken token = default) {
        List<Response> events = [];
        await foreach (ReadOnlyMemory<byte> frame in transport.ExchangeAsync(DeviceCommand.Execute, job, token)) {
            Response decoded = ResponseDecoder.Decode(frame.Span);
            events.Add(decoded);
            if (decoded.Type == 3) break;
        }
        int results = events.Count(value => value.Type == 3);
        if (results != 1) throw new IOException("Execution did not produce exactly one RESULT.");
        if (events.Count > 1 && events[0].Type != 1) throw new IOException("Accepted execution did not begin with ACK.");
        if (events.Count == 1 && events[0].Type != 3) throw new IOException("Validation rejection was not explicit.");
        return new(events);
    }
}
