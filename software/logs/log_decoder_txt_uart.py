import struct

INPUT_FILE = "decoded/uart0.bin"
OUTPUT_FILE = "decoded/uart0.txt"

TS_FMT = "<Q"
TS_SIZE = struct.calcsize(TS_FMT)

def fmt_payload(buf):
    out = []
    for b in buf:
        if b == 0:
            out.append("\\0")
        elif 32 <= b <= 126:
            out.append(chr(b))
        else:
            out.append(f"\\x{b:02X}")
    return "".join(out)

with open(INPUT_FILE, "rb") as f:
    data = f.read()

offset = 0
idx = 0

with open(OUTPUT_FILE, "w") as out:
    while offset + TS_SIZE < len(data):
        ts = struct.unpack_from(TS_FMT, data, offset)[0]
        offset += TS_SIZE

        payload = bytearray()
        while offset < len(data):
            b = data[offset]
            offset += 1
            payload.append(b)
            if b == 0:
                break

        out.write(f"[{idx:05d}] {ts} us | {fmt_payload(payload)}\n")
        idx += 1

print("UART TXT gerado.")
