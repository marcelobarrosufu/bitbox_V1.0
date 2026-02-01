import struct
import os
import mmap
import glob
import argparse
import shutil

# ========= CONFIGURAÇÕES DO PROTOCOLO =========
HEADER_MAGIC = 0xDEADBEEF
SD_LOG_UART = 1
SD_LOG_GPIO = 2

# Structs
HDR_FMT = "<I Q B B"     # header, time_us, log_type, periph_num
HDR_SIZE = struct.calcsize(HDR_FMT)
UART_LEN_FMT = "<H"
UART_LEN_SIZE = struct.calcsize(UART_LEN_FMT)
GPIO_FMT = "<B B"
GPIO_SIZE = struct.calcsize(GPIO_FMT)

# Formatos de Decode (TXT)
GPIO_TXT_FMT = "<Q B B"
UART_TXT_TS_FMT = "<Q"

# ========= FUNÇÕES DE DECODE (TXT) =========

def fmt_payload_uart(buf):
    out = []
    for b in buf:
        if b == 0: out.append("\\0")
        elif 32 <= b <= 126: out.append(chr(b))
        else: out.append(f"\\x{b:02X}")
    return "".join(out)

def decode_to_txt(input_dir, output_dir):
    print(f"--- Iniciando conversão para TXT em '{output_dir}' ---")
    bin_files = glob.glob(os.path.join(input_dir, "*.bin"))

    for bin_file in bin_files:
        filename = os.path.basename(bin_file)
        name_no_ext = os.path.splitext(filename)[0]
        txt_path = os.path.join(output_dir, name_no_ext + ".txt")

        if filename.startswith("gpio"):
            # Lógica GPIO TXT
            with open(bin_file, "rb") as f_in, open(txt_path, "w") as f_out:
                f_out.write("Timestamp (us) | Edge | Level\n" + "-"*35 + "\n")
                idx = 0
                while chunk := f_in.read(struct.calcsize(GPIO_TXT_FMT)):
                    ts, edge, level = struct.unpack(GPIO_TXT_FMT, chunk)
                    f_out.write(f"[{idx:05d}] {ts:<12} | edge={edge} level={level}\n")
                    idx += 1
            print(f"Gerado: {filename} -> .txt")

        elif filename.startswith("uart"):
            # Lógica UART TXT
            with open(bin_file, "rb") as f_in, open(txt_path, "w") as f_out:
                f_out.write("Timestamp (us) | Payload\n" + "-"*35 + "\n")
                idx = 0
                ts_size = struct.calcsize(UART_TXT_TS_FMT)
                while True:
                    ts_data = f_in.read(ts_size)
                    if not ts_data: break
                    ts = struct.unpack(UART_TXT_TS_FMT, ts_data)[0]
                    
                    payload = bytearray()
                    while (byte := f_in.read(1)):
                        payload.append(byte[0])
                        if byte[0] == 0: break
                    
                    f_out.write(f"[{idx:05d}] {ts:<12} | {fmt_payload_uart(payload)}\n")
                    idx += 1
            print(f"Gerado: {filename} -> .txt")

# ========= PROCESSADOR PRINCIPAL (SPLITTER) =========

def process_log(input_file, output_dir, keep_bins=True):
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    print(f"Abrindo arquivo de log: {input_file}")
    
    # Dicionário para manter handles de arquivo abertos
    out_files = {}

    def get_file_handle(prefix, num):
        key = f"{prefix}{num}"
        if key not in out_files:
            path = os.path.join(output_dir, f"{key}.bin")
            out_files[key] = open(path, "wb") # wb apaga o anterior
        return out_files[key]

    # Usando mmap para performance e economia de RAM
    with open(input_file, "rb") as f:
        # Se arquivo vazio, aborta
        if os.path.getsize(input_file) == 0:
            print("Arquivo vazio.")
            return

        with mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as mm:
            offset = 0
            total_size = len(mm)
            
            while offset + HDR_SIZE <= total_size:
                # Otimização: Busca rápida pelo header magic
                # Se não estiver alinhado, usa .find para pular lixo
                check_magic = struct.unpack_from("<I", mm, offset)[0]
                
                if check_magic != HEADER_MAGIC:
                    # Tenta achar o próximo magic byte a byte (lento) ou via find (rápido)
                    new_offset = mm.find(struct.pack("<I", HEADER_MAGIC), offset + 1)
                    if new_offset == -1:
                        break # Fim dos dados válidos
                    offset = new_offset
                    continue

                # Header válido encontrado
                try:
                    _, time_us, log_type, periph = struct.unpack_from(HDR_FMT, mm, offset)
                except:
                    break
                
                p = offset + HDR_SIZE

                # --- UART ---
                if log_type == SD_LOG_UART:
                    if p + UART_LEN_SIZE > total_size: break
                    payload_len = struct.unpack_from(UART_LEN_FMT, mm, p)[0]
                    p += UART_LEN_SIZE
                    
                    if payload_len > 1024 or p + payload_len > total_size:
                        offset += 1 # Payload inválido, corrupção?
                        continue
                        
                    payload = mm[p : p + payload_len]
                    
                    f_out = get_file_handle("uart", periph)
                    f_out.write(struct.pack("<Q", time_us))
                    f_out.write(payload)
                    f_out.write(b"\x00") # Null terminator pro decoder
                    
                    offset = p + payload_len

                # --- GPIO ---
                elif log_type == SD_LOG_GPIO:
                    if p + GPIO_SIZE > total_size: break
                    edge, level = struct.unpack_from(GPIO_FMT, mm, p)
                    
                    f_out = get_file_handle("gpio", periph)
                    f_out.write(struct.pack("<Q B B", time_us, edge, level))
                    
                    offset = p + GPIO_SIZE
                
                else:
                    # Tipo desconhecido, avança 1 byte para tentar resincronizar
                    offset += 1

    # Fecha todos os arquivos binários gerados
    for f_h in out_files.values():
        f_h.close()
    
    print("Separação concluída.")
    
    # Chama o decoder para TXT
    decode_to_txt(output_dir, output_dir)

    # Limpeza (Opcional)
    if not keep_bins:
        print("Removendo binários temporários...")
        for k in out_files.keys():
            try: os.remove(os.path.join(output_dir, k + ".bin"))
            except: pass

# ========= ENTRY POINT =========

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="BitBox Log Processor")
    parser.add_argument("logfile", help="Caminho do arquivo .BIN (ex: LOG12.BIN)")
    parser.add_argument("--out", default="decoded_logs", help="Pasta de saída")
    parser.add_argument("--clean", action="store_true", help="Apagar .bin intermediários, manter só .txt")
    
    args = parser.parse_args()
    
    process_log(args.logfile, args.out, keep_bins=not args.clean)