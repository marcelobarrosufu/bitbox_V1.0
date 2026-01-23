import struct
import os

# ========= CONFIG =========

INPUT_FILE = "LOG1.BIN"
OUTPUT_DIR = "decoded"

HEADER_MAGIC = 0xDEADBEEF
UART_MAX_PAYLOAD_LEN = 1024

SD_LOG_UART = 1
SD_LOG_GPIO = 2

# Structs (packed, little-endian)
HDR_FMT = "<I Q B B"     # header, time_us, log_type, periph_num
HDR_SIZE = struct.calcsize(HDR_FMT)

UART_LEN_FMT = "<H"
UART_LEN_SIZE = struct.calcsize(UART_LEN_FMT)

GPIO_FMT = "<B B"
GPIO_SIZE = struct.calcsize(GPIO_FMT)

os.makedirs(OUTPUT_DIR, exist_ok=True)

uart_files = {}
gpio_files = {}

# ========= HELPERS =========

def uart_file(n):
    if n not in uart_files:
        uart_files[n] = open(f"{OUTPUT_DIR}/uart{n}.bin", "ab")
    return uart_files[n]

def gpio_file(n):
    if n not in gpio_files:
        gpio_files[n] = open(f"{OUTPUT_DIR}/gpio{n}.bin", "ab")
    return gpio_files[n]

# ========= DECODER =========

with open(INPUT_FILE, "rb") as f:
    data = f.read()

offset = 0
total = len(data)

while offset + HDR_SIZE <= total:
    header = struct.unpack_from("<I", data, offset)[0]

    if header != HEADER_MAGIC:
        offset += 1
        continue

    try:
        _, time_us, log_type, periph = struct.unpack_from(HDR_FMT, data, offset)
    except struct.error:
        break

    p = offset + HDR_SIZE

    # ===== UART =====
    if log_type == SD_LOG_UART:
        if p + UART_LEN_SIZE > total:
            break

        payload_len = struct.unpack_from(UART_LEN_FMT, data, p)[0]
        p += UART_LEN_SIZE

        if payload_len > UART_MAX_PAYLOAD_LEN:
            offset += 1
            continue

        if p + payload_len > total:
            break

        payload = data[p:p + payload_len]

        out = uart_file(periph)
        out.write(struct.pack("<Q", time_us))
        out.write(payload)
        out.write(b"\x00")

        offset = p + payload_len
        continue

    # ===== GPIO =====
    elif log_type == SD_LOG_GPIO:
        if p + GPIO_SIZE > total:
            break

        edge, level = struct.unpack_from(GPIO_FMT, data, p)

        out = gpio_file(periph)
        out.write(struct.pack("<Q B B", time_us, edge, level))

        offset = p + GPIO_SIZE
        continue

    else:
        offset += 1

# ========= FECHAMENTO =========

for f in uart_files.values():
    f.close()

for f in gpio_files.values():
    f.close()

print("Stage 1 concluído: binários normalizados gerados.")
