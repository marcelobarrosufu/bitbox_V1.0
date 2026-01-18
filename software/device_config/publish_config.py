import yaml
import json
import time
import ssl
import paho.mqtt.client as mqtt

# ---------- MQTT ----------
BROKER_URL = "qa717179.ala.us-east-1.emqxsl.com"
BROKER_PORT = 8883
USERNAME = "luizpedrobt"
PASSWORD = "papagaio23"
CA_CERT_PATH = "../mqtt/emqxsl-ca.crt"

TOPIC_GPIO = "topic/config/gpio"
TOPIC_UART = "topic/config/uart"

# ---------- ENUMERAÇÕES GPIO (FW) ----------
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

# GPIO físico → enum do firmware
GPIO_TO_ENUM = {
    1:  0,
    2:  1,
    3:  2,
    4:  3,
    5:  4,
    33: 5,
    34: 6,
    35: 7,
    36: 8,
    37: 9,
}

# ---------- MQTT ----------
def on_connect(client, userdata, flags, rc):
    if rc != 0:
        raise RuntimeError(f"Erro MQTT rc={rc}")
    print("MQTT conectado")

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

# ---------- LOAD CONFIG ----------
with open("data_logger_config.yaml", "r") as f:
    cfg = yaml.safe_load(f)

# ================= GPIO =================
for g in cfg.get("gpio", []):
    gpio = g["gpio"]

    if gpio not in GPIO_TO_ENUM:
        raise ValueError(f"GPIO inválido: {gpio}")

    payload = {
        "state": g["state"],
        "gpio_num": GPIO_TO_ENUM[gpio],      
        "mode": GPIO_MODE[g["mode"]],
        "pull_up_en": GPIO_PULL[g["pullup"]],
        "pull_down_en": GPIO_PULL[g["pulldown"]],
        "intr_type": GPIO_INTR[g["intr"]],
    }

    print(f"[GPIO] {payload}")
    client.publish(TOPIC_GPIO, json.dumps(payload), qos=1)
    time.sleep(0.2)

# ================= UART =================
for u in cfg.get("uart", []):
    for pin in (u["rx_gpio"], u["tx_gpio"]):
        if pin not in GPIO_TO_ENUM:
            raise ValueError(f"GPIO inválido em UART: {pin}")

    payload = {
        "state": u["state"],
        "uart_num": u["uart_num"],
        "rx_gpio": u["rx_gpio"],
        "tx_gpio": u["tx_gpio"],
        "baudrate": u["baudrate"],
    }

    print(f"[UART] {payload}")
    client.publish(TOPIC_UART, json.dumps(payload), qos=1)
    time.sleep(0.2)

client.loop_stop()
client.disconnect()

print("Configuração enviada com sucesso.")
