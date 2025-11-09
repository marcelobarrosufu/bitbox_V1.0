## 🗓️ 09/11/2025 – Teste de Desempenho UART + SDMMC

### 🔧 O que foi feito
- Implementado e testado o **sistema de buffer duplo** (100 kB cada) para recepção UART.
- Taxa configurada: **115200 bps**.
- Captura e medição de tempo via `esp_timer_get_time()`.

### ⚙️ Resultados
| Etapa | Descrição | Tempo (aprox.) | Observações |
|:------|:-----------|:----------------|:-------------|
| UART RX | Tempo para preencher 100 kB | **8,6 s** | Consistente com a taxa de transmissão configurada |
| SDMMC Write | Tempo para armazenar 100 kB em FATFS | **1,8 s (achei estranho pela velocidade da interface)** | Escrita realizada via interface SDMMC (4-bit, 40 MHz) |

### 🧩 Conclusões
- O sistema de recepção e gravação estável e funcional.
- A escrita no cartão SD foi bem-sucedida, com tempo coerente considerando overhead do FATFS.
- O buffer duplo permitiu recepção contínua sem perdas durante a gravação.

### 🚀 Próximos Passos
- [ ] Implementar **publicação MQTT** em broker remoto para transmissão dos dados armazenados.
- [ ] Começar o design da **interface da aplicação final** (visual e lógica).
- [ ] Avaliar possíveis melhorias na taxa de gravação SD (maior clock ou DMA).

---

📅 *Registro:* 09/11/2025  
✍️ *Autor:* Luiz Pedro
🧩 *Projeto:* BitBox – Sistema de Aquisição UART + SDMMC + MQTT
