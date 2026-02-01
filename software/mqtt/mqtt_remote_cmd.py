import argparse
import json
import time
import sys
import ssl
import os

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Erro: Biblioteca 'paho-mqtt' não encontrada.")
    print("Instale usando: pip install paho-mqtt")
    sys.exit(1)

# ==========================================
# CONFIGURAÇÕES ESTÁTICAS
# ==========================================
BROKER_URL   = "qa717179.ala.us-east-1.emqxsl.com"
BROKER_PORT  = 8883
USERNAME     = "luizpedrobt"
PASSWORD     = "papagaio23"

# Caminho para o certificado CA (Server Certificate)
# Certifique-se que este arquivo existe relativo a onde você roda o script
CA_CERT_PATH = "../mqtt/emqxsl-ca.crt"

# Tópico Base (Deve bater com o firmware)
TOPIC_BASE   = "datalogger/cmd"

# ==========================================
# FUNÇÕES DE COMANDO
# ==========================================

def cmd_gpio(args, client):
    """Envia comando para GPIO"""
    topic = f"{TOPIC_BASE}/gpio"
    
    # Payload JSON: {"pin_index": 0, "level": 1}
    payload = {
        "pin_index": args.index,
        "level": args.level
    }
    
    json_str = json.dumps(payload)
    print(f"[GPIO] Enviando para '{topic}': {json_str}")
    
    info = client.publish(topic, json_str, qos=1)
    info.wait_for_publish()
    print("-> Comando entregue ao Broker.")

def cmd_uart(args, client):
    """Envia comando para UART"""
    topic = f"{TOPIC_BASE}/uart"
    
    # Trata caracteres de escape (permite enviar \r\n via terminal)
    msg_content = args.msg.encode('utf-8').decode('unicode_escape')

    # Payload JSON: {"uart_num": 1, "msg": "Ola"}
    payload = {
        "uart_num": args.uart,
        "msg": msg_content
    }
    
    json_str = json.dumps(payload)
    print(f"[UART] Enviando para '{topic}': {json_str}")
    
    info = client.publish(topic, json_str, qos=1)
    info.wait_for_publish()
    print("-> Comando entregue ao Broker.")

# ==========================================
# CALLBACKS MQTT
# ==========================================

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"Conectado com sucesso a {BROKER_URL}!")
    else:
        print(f"Falha na conexão. Código de retorno: {rc}")

# ==========================================
# MAIN
# ==========================================

def main():
    # Parser apenas para os comandos (GPIO/UART), sem config de rede
    parser = argparse.ArgumentParser(description="BitBox Remote Controller")
    subparsers = parser.add_subparsers(dest="command", required=True, help="Tipo de comando")

    # --- Subcomando GPIO ---
    # Ex: python remote_cmd.py gpio 0 1
    p_gpio = subparsers.add_parser("gpio", help="Controlar nível lógico de GPIO")
    p_gpio.add_argument("index", type=int, help="Índice Lógico do Pino (0-9)")
    p_gpio.add_argument("level", type=int, choices=[0, 1], help="Nível: 0 (LOW) ou 1 (HIGH)")
    p_gpio.set_defaults(func=cmd_gpio)

    # --- Subcomando UART ---
    # Ex: python remote_cmd.py uart 1 "AT\r\n"
    p_uart = subparsers.add_parser("uart", help="Enviar texto via UART")
    p_uart.add_argument("uart", type=int, help="Número da UART (0-2)")
    p_uart.add_argument("msg", type=str, help="Mensagem a enviar")
    p_uart.set_defaults(func=cmd_uart)

    args = parser.parse_args()

    # Configuração do Cliente
    client_id = f"cmd_sender_{int(time.time())}"
    client = mqtt.Client(client_id=client_id, protocol=mqtt.MQTTv311)
    
    # Autenticação
    client.username_pw_set(USERNAME, PASSWORD)

    # Configuração SSL/TLS
    print("Configurando contexto seguro (SSL/TLS)...")
    context = ssl.create_default_context()
    
    # Verifica se o certificado existe antes de carregar
    if os.path.exists(CA_CERT_PATH):
        try:
            context.load_verify_locations(cafile=CA_CERT_PATH)
            print(f"Certificado CA carregado de: {CA_CERT_PATH}")
        except Exception as e:
            print(f"Erro ao carregar certificado: {e}")
            sys.exit(1)
    else:
        print(f"AVISO: Arquivo de certificado '{CA_CERT_PATH}' não encontrado!")
        print("Tentando conectar usando certificados padrão do sistema...")
        # Se não achar o arquivo, tenta usar o default do sistema (pode falhar com EMQX privado)
    
    client.tls_set_context(context)
    client.on_connect = on_connect

    try:
        print(f"Conectando...")
        client.connect(BROKER_URL, BROKER_PORT, 60)
        client.loop_start() 
        
        # Aguarda conexão
        time.sleep(1.5) 
        
        # Executa a função do comando escolhido
        args.func(args, client)
        
        # Aguarda transmissão
        time.sleep(0.5)
        
        client.loop_stop()
        client.disconnect()

    except Exception as e:
        print(f"Erro Crítico de Execução: {e}")

if __name__ == "__main__":
    main()