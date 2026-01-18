# enviar_pacotes.py
import serial
import time
import random
import os

PORT = "/dev/ttyACM1"    # ajuste se necessário
BAUD = 115200

MIN_SIZE = 10           # tamanho mínimo do payload (bytes)
MAX_SIZE = 900          # tamanho máximo (antes do 0x00)
PERIOD = 5.0            # segundos entre pacotes

random.seed()           # seed automática

with serial.Serial(PORT, BAUD, timeout=5) as s:
    time.sleep(0.1)  # tempo para abrir a porta

    print("Enviando pacotes de tamanho variável terminados em \\0 ...")

    while True:
        # tamanho pseudo-aleatório do payload
        payload_len = random.randint(MIN_SIZE, MAX_SIZE)

        # gera payload binário arbitrário (sem 0x00)
        payload = bytearray()
        while len(payload) < payload_len:
            b = random.randint(1, 255)  # evita 0x00
            payload.append(b)

        # adiciona delimitador de frame
        payload.append(0x00)

        t0 = time.perf_counter()

        s.write(payload)
        s.flush()

        t1 = time.perf_counter()
        print(f"Frame enviado: {payload_len} bytes + 0x00 "
              f"({t1 - t0:.6f} s)")

        time.sleep(PERIOD)
