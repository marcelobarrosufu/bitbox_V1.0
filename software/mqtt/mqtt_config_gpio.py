import json
import time
import ssl
import argparse
import paho.mqtt.client as mqtt

# --- CONFIGURAÇÕES MQTT ---
BROKER_URL = "qa717179.ala.us-east-1.emqxsl.com"
BROKER_PORT = 8883
USERNAME = "luizpedrobt"
PASSWORD = "papagaio23"
TOPIC = "topic/config/gpio"
CA_CERT_PATH = "emqxsl-ca.crt"

# --- ENUMERAÇÕES (ESP-IDF / FW) ---
GPIO_MODE = {
    "DISABLE": 0,
    "INPUT": 1,
    "OUTPUT": 2,
    "INPUT_OUTPUT": 3,
}

GPIO_PULL = {
    "DISABLE": 0,
    "ENABLE": 1,
}

GPIO_INTR = {
    "DISABLE": 0,
    "POSEDGE": 1,
    "NEGEDGE": 2,
    "ANYEDGE": 3,
    "LOW_LEVEL": 4,
    "HIGH_LEVEL": 5,
}

# --- MAPA GPIO FÍSICO -> ENUM DO FW ---
GPIO_TO_ENUM = {
    1:  0,
    2:  1,
    3:  2,
    4:  3,
    5:  4,
    33: 5,
    34: 6,
    35: 7,
    37: 8,
    38: 9,
}

# ---------- CLI ----------
parser = argparse.ArgumentParser(
    description="Publica configuração de GPIO via MQTT (entrada em GPIO físico)"
)

parser.add_argument(
    "--gpio",
    type=int,
    nargs="+",
    required=True,
    help="GPIO(s) físicos a configurar (ex: 1 2 33 38)"
)

parser.add_argument(
    "--mode",
    choices=GPIO_MODE.keys(),
    default="INPUT",
    help="Modo do GPIO"
)

parser.add_argument(
    "--intr",
    choices=GPIO_INTR.keys(),
    default="ANYEDGE",
    help="Tipo de interrupção"
)

parser.add_argument(
    "--pullup",
    choices=GPIO_PULL.keys(),
    default="ENABLE",
    help="Pull-up"
)

parser.add_argument(
    "--pulldown",
    choices=GPIO_PULL.keys(),
    default="DISABLE",
    help="Pull-down"
)

parser.add_argument(
    "--state",
    type=int,
    choices=[0, 1],
    default=1,
    help="Habilita (1) ou desabilita (0) o GPIO"
)

args = parser.parse_args()

# ---------- VALIDAÇÃO ----------
for gpio in args.gpio:
    if gpio not in GPIO_TO_ENUM:
        raise ValueError(f"GPIO físico inválido ou não suportado: {gpio}")

# ---------- MQTT CALLBACK ----------
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Conectado ao broker MQTT")
    else:
        print(f"Erro ao conectar no MQTT (rc={rc})")

# ---------- MQTT SETUP ----------
client = mqtt.Client()
client.username_pw_set(USERNAME, PASSWORD)
client.on_connect = on_connect

client.tls_set(
    ca_certs=CA_CERT_PATH,
    tls_version=ssl.PROTOCOL_TLSv1_2
)

client.connect(BROKER_URL, BROKER_PORT, keepalive=60)
client.loop_start()

time.sleep(1)

# ---------- PUBLICAÇÃO ----------
for gpio in args.gpio:
    gpio_enum = GPIO_TO_ENUM[gpio]

    payload = {
        "state": args.state,
        "gpio_num": gpio_enum,                 # <<< ENUM DO FW
        "mode": GPIO_MODE[args.mode],
        "pull_up_en": GPIO_PULL[args.pullup],
        "pull_down_en": GPIO_PULL[args.pulldown],
        "intr_type": GPIO_INTR[args.intr],
    }

    print(
        f"Publicando GPIO físico {gpio} "
        f"(enum {gpio_enum}) -> {json.dumps(payload)}"
    )

    client.publish(TOPIC, json.dumps(payload), qos=1)
    time.sleep(0.3)

# ---------- FINALIZAÇÃO ----------
client.loop_stop()
client.disconnect()

print("Configuração enviada com sucesso.")
