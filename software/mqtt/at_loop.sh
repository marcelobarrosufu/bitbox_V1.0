#!/bin/bash
cd ../
source venv/bin/activate

cd mqtt/

# Pede número da UART
read -p "Digite o numero da UART: " UART

if [ -z "$UART" ]; then
    echo "UART invalida."
    exit 1
fi

echo "Usando UART $UART"
echo "Digite comandos AT (Ctrl+C para sair)"
echo "---------------------------------------"

while true; do
    read -p "> " CMD

    # Ignora linha vazia
    if [ -z "$CMD" ]; then
        continue
    fi

    # Chama o python adicionando \r\n automaticamente
    python3 mqtt_remote_cmd.py uart "$UART" "${CMD}\r\n"
done
