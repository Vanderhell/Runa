using System.Buffers.Binary;
using System.Runtime.CompilerServices;

namespace Runa.Transport.Serial;

public sealed class WindowsSerialHardwareTransport : IHardwareTransport {
    private readonly FileStream stream;
    private readonly TimeSpan timeout;

    public WindowsSerialHardwareTransport(string port, TimeSpan? timeout = null) {
        if (!OperatingSystem.IsWindows()) throw new PlatformNotSupportedException();
        if (string.IsNullOrWhiteSpace(port)) throw new ArgumentException("Port is required.", nameof(port));
        stream = new FileStream($@"\\.\{port}", FileMode.Open, FileAccess.ReadWrite,
                                FileShare.None, 4096, FileOptions.Asynchronous);
        this.timeout = timeout ?? TimeSpan.FromSeconds(3);
    }

    public async IAsyncEnumerable<ReadOnlyMemory<byte>> ExchangeAsync(
        DeviceCommand command, ReadOnlyMemory<byte> payload,
        [EnumeratorCancellation] CancellationToken cancellationToken) {
        if (payload.Length > 2048) throw new ArgumentOutOfRangeException(nameof(payload));
        byte[] request = new byte[8 + payload.Length];
        "JREQ"u8.CopyTo(request);
        request[4] = 1;
        request[5] = (byte)command;
        BinaryPrimitives.WriteUInt16LittleEndian(request.AsSpan(6), checked((ushort)payload.Length));
        payload.CopyTo(request.AsMemory(8));
        await stream.WriteAsync(request, cancellationToken);
        await stream.FlushAsync(cancellationToken);
        do {
            byte[] frame = await ReadFrame(cancellationToken);
            yield return frame;
            if (command != DeviceCommand.Execute || frame[0] == 3) break;
        } while (true);
    }

    public async Task SendMalformedAsync(ReadOnlyMemory<byte> bytes, CancellationToken token = default) {
        await stream.WriteAsync(bytes, token);
        await stream.FlushAsync(token);
    }

    private async Task<byte[]> ReadFrame(CancellationToken outer) {
        using var deadline = CancellationTokenSource.CreateLinkedTokenSource(outer);
        deadline.CancelAfter(timeout);
        byte[] header = new byte[4];
        int matched = 0;
        while (matched < 2) {
            byte value = await ReadByte(deadline.Token);
            if (matched == 0) {
                if (value is 1 or 2 or 3 or 0x10 or 0x11 or 0x12) {
                    header[0] = value;
                    matched = 1;
                }
            } else if (value == 1) {
                header[1] = value;
                matched = 2;
            } else {
                matched = 0;
            }
        }
        await ReadExact(header.AsMemory(2, 2), deadline.Token);
        ushort size = BinaryPrimitives.ReadUInt16LittleEndian(header.AsSpan(2));
        if (size < 8 || size > 2048 + 8) throw new IOException($"Invalid device frame size {size}.");
        byte[] frame = new byte[size];
        header.CopyTo(frame, 0);
        await ReadExact(frame.AsMemory(4), deadline.Token);
        return frame;
    }

    private async Task<byte> ReadByte(CancellationToken token) {
        byte[] one = new byte[1];
        await ReadExact(one, token);
        return one[0];
    }

    private async Task ReadExact(Memory<byte> buffer, CancellationToken token) {
        int offset = 0;
        while (offset < buffer.Length) {
            int count = await stream.ReadAsync(buffer[offset..], token);
            if (count == 0) throw new EndOfStreamException();
            offset += count;
        }
    }

    public ValueTask DisposeAsync() => stream.DisposeAsync();
}
