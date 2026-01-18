import paho.mqtt.client as mqtt
from datetime import datetime
import ssl
import os

# ================== CONFIG ==================
BROKER_URL  = "qa717179.ala.us-east-1.emqxsl.com"
BROKER_PORT = 8883

USERNAME = "luizpedrobt"
PASSWORD = "papagaio23"

CA_CERT_PATH = "emqxsl-ca.crt"

TOPICS = [
    ("datalogger/gpio/+", 0),
    ("datalogger/uart/+", 0),
]

CLIENT_ID = "datalogger_python_sub"

LOG_DIR = "logs"
# ============================================


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("[MQTT] Conectado com sucesso (TLS)")
        for topic, qos in TOPICS:
            client.subscribe(topic, qos)
            print(f"[MQTT] Subscrito em: {topic}")
    else:
        print(f"[MQTT] Falha na conexão, rc={rc}")


def on_disconnect(client, userdata, rc):
    print("[MQTT] Desconectado do broker")


def on_message(client, userdata, msg):
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
    topic = msg.topic

    # --- trate payload como binário por padrão ---
    payload = msg.payload

    try:
        payload_str = payload.decode("utf-8")
        printable = payload_str
    except UnicodeDecodeError:
        printable = payload.hex(" ")

    line = f"[{timestamp}] {topic}: {printable}"

    print(line)
    save_log(topic, line)


def save_log(topic, line):
    os.makedirs(LOG_DIR, exist_ok=True)

    safe_topic = topic.replace("/", "_")
    filename = f"{safe_topic}.txt"
    path = os.path.join(LOG_DIR, filename)

    with open(path, "a", encoding="utf-8") as f:
        f.write(line + "\n")


def main():
    client = mqtt.Client(client_id=CLIENT_ID)

    # Auth
    client.username_pw_set(USERNAME, PASSWORD)

    # TLS
    client.tls_set(
        ca_certs=CA_CERT_PATH,
        certfile=None,
        keyfile=None,
        tls_version=ssl.PROTOCOL_TLSv1_2
    )

    client.tls_insecure_set(False)

    # Callbacks
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message

    print("[MQTT] Conectando ao broker EMQX Cloud...")
    client.connect(BROKER_URL, BROKER_PORT, keepalive=60)

    client.loop_forever()


if __name__ == "__main__":
    main()
