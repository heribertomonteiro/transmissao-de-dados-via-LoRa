#include "lora_RFM95.h"
#include <stdio.h>
#include <string.h>
#include <generated/csr.h>
#include <system.h>

#define TX_TIMEOUT_MS 5000
#define SPI_MODE_MANUAL (1 << 16)
#define SPI_CS_MASK     0x0001 
#define REG_FIFO                 0x00
#define REG_OP_MODE              0x01
#define REG_FRF_MSB              0x06
#define REG_FRF_MID              0x07
#define REG_FRF_LSB              0x08
#define REG_PA_CONFIG            0x09
#define REG_LNA                  0x0C
#define REG_FIFO_ADDR_PTR        0x0D
#define REG_FIFO_TX_BASE_ADDR    0x0E
#define REG_FIFO_RX_BASE_ADDR    0x0F
#define REG_IRQ_FLAGS_MASK       0x11
#define REG_IRQ_FLAGS            0x12
#define REG_MODEM_CONFIG_1       0x1D
#define REG_MODEM_CONFIG_2       0x1E
#define REG_PREAMBLE_MSB         0x20
#define REG_PREAMBLE_LSB         0x21
#define REG_PAYLOAD_LENGTH       0x22
#define REG_MODEM_CONFIG_3       0x26
#define REG_SYNC_WORD            0x39
#define REG_DIO_MAPPING_1        0x40
#define REG_VERSION              0x42
#define REG_PA_DAC               0x4D
#define REG_OCP                  0x0B
#define MODE_SLEEP               0x00
#define MODE_STDBY               0x01
#define MODE_TX                  0x03
#define IRQ_TX_DONE_MASK         0x08

static void busy_wait_ms_local(unsigned int ms);
static void spi_master_init(void);
static inline void spi_select(void);
static inline void spi_deselect(void);
static inline uint8_t spi_txrx(uint8_t tx_byte);
static void lora_write_fifo(const uint8_t *data, uint8_t len);

static void busy_wait_ms_local(unsigned int ms) {
    for (unsigned int i = 0; i < ms; ++i) {
#ifdef CSR_TIMER0_BASE
        busy_wait_us(1000);
#else
        // Fallback simples se o timer0 não estiver definido
        for(volatile int j = 0; j < 2000; j++); // Ajuste este valor conforme necessário para sua CPU/Clock
#endif
    }
}

static void spi_master_init(void) {
    spi_cs_write(SPI_MODE_MANUAL | 0x0000);
    #ifdef CSR_SPI_LOOPBACK_ADDR
    spi_loopback_write(0);
    #endif
    busy_wait_ms_local(1);
}

static inline void spi_select(void) {
    spi_cs_write(SPI_MODE_MANUAL | SPI_CS_MASK);
    busy_wait_us(2);
}

static inline void spi_deselect(void) {
    spi_cs_write(SPI_MODE_MANUAL | 0x0000);
    busy_wait_us(2);
}

static inline uint8_t spi_txrx(uint8_t tx_byte) {
    uint32_t rx_byte;

    spi_mosi_write((uint32_t)tx_byte);
    spi_control_write(
        (1 << CSR_SPI_CONTROL_START_OFFSET) |
        (8 << CSR_SPI_CONTROL_LENGTH_OFFSET)
    );
    while( (spi_status_read() & (1 << CSR_SPI_STATUS_DONE_OFFSET)) == 0 ) {
    }
    rx_byte = spi_miso_read();
    return (uint8_t)(rx_byte & 0xFF);
}


static void lora_write_fifo(const uint8_t *data, uint8_t len) {
    spi_select();
    spi_txrx(REG_FIFO | 0x80);
    for (uint8_t i = 0; i < len; i++) {
        spi_txrx(data[i]);
    }
    spi_deselect();
}

uint8_t lora_read_reg(uint8_t reg) {
    uint8_t val;
    spi_select();
    spi_txrx(reg & 0x7F); // Endereço com bit de escrita em 0
    val = spi_txrx(0x00);
    spi_deselect();
    return val;
}

// Escreve registrador (pública)
void lora_write_reg(uint8_t reg, uint8_t value) {
    spi_select();
    spi_txrx(reg | 0x80); // Endereço com bit de escrita em 1
    spi_txrx(value);
    spi_deselect();
}

// Define modo (pública)
void lora_set_mode(uint8_t mode) {
    // O bit 7 (LongRangeMode) deve estar sempre 1 para LoRa
    lora_write_reg(REG_OP_MODE, (0x80 | mode));
}

// Inicializa LoRa (pública)
bool lora_init(void) {
    // 1. Inicializa o barramento SPI primeiro
    spi_master_init();

    uint8_t rx;

    // 2. Reseta o módulo LoRa (se o pino de reset estiver disponível no CSR)
    #ifdef CSR_LORA_RESET_BASE
    lora_reset_out_write(0); busy_wait_ms_local(5);
    lora_reset_out_write(1); busy_wait_ms_local(10);
    #endif

    // 3. Verifica a versão do chip via SPI
    rx = lora_read_reg(REG_VERSION);
    if (rx != 0x12) {
        printf("Falha na comunicação\n");
        return false; // Falha na inicialização
    }

    // 4. Configurações do rádio (idênticas às do código anterior)
    lora_set_mode(MODE_SLEEP);

    uint64_t frf = ((uint64_t)915000000 << 19) / 32000000; // 915 MHz
    lora_write_reg(REG_FRF_MSB, (uint8_t)(frf >> 16)); 
    lora_write_reg(REG_FRF_MID, (uint8_t)(frf >> 8));
    lora_write_reg(REG_FRF_LSB, (uint8_t)(frf >> 0));
    lora_write_reg(REG_PA_CONFIG, 0xFF); 
    lora_write_reg(REG_PA_DAC, 0x87);
    lora_write_reg(REG_MODEM_CONFIG_1, 0x78); 
    lora_write_reg(REG_MODEM_CONFIG_2, 0xC4); 
    lora_write_reg(REG_MODEM_CONFIG_3, 0x0C);
    lora_write_reg(REG_PREAMBLE_MSB, 0x00);
    lora_write_reg(REG_PREAMBLE_LSB, 0x0C);
    lora_write_reg(REG_SYNC_WORD, 0x12);  
    lora_write_reg(REG_OCP, 0x37);  
    lora_write_reg(REG_FIFO_TX_BASE_ADDR, 0x00);
    lora_write_reg(REG_FIFO_RX_BASE_ADDR, 0x00);
    lora_write_reg(REG_LNA, 0x23);      
    lora_write_reg(REG_IRQ_FLAGS_MASK, 0x00); 
    lora_write_reg(REG_IRQ_FLAGS, 0xFF);
    lora_set_mode(MODE_STDBY);
    busy_wait_ms_local(10);

    printf("Modulacao: BW=62.5kHz, SF=12, CR=4/8, Preamble=12, SyncWord=0x12\n");

    return true; 
}


// Envia bytes
bool lora_send_bytes(const uint8_t *data, size_t len) {
    if (len == 0 || len > 255) {
        printf("Erro LoRa: Tamanho do pacote inválido (%d bytes)\n", (int)len);
        return false;
    }

    // Garante que está em Standby antes de começar
    lora_set_mode(MODE_STDBY);

    // Configura ponteiro FIFO e escreve os dados
    lora_write_reg(REG_FIFO_ADDR_PTR, 0x00);
    lora_write_fifo(data, (uint8_t)len);
    lora_write_reg(REG_PAYLOAD_LENGTH, (uint8_t)len);

    // Prepara para TX: limpa flags e mapeia DIO0 para TxDone
    lora_write_reg(REG_IRQ_FLAGS, 0xFF);
    lora_write_reg(REG_DIO_MAPPING_1, 0x40); // DIO0 = 01 (TxDone)

    printf("Enviando %d bytes via LoRa...\n", (int)len);

    // Inicia a transmissão
    lora_set_mode(MODE_TX);

    // Espera pelo TxDone (IRQ_TX_DONE_MASK = 0x08) com timeout
    int timeout_cnt = TX_TIMEOUT_MS;
    while (timeout_cnt > 0) {
        // Polling na flag IRQ
        if (lora_read_reg(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) {
            lora_write_reg(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK); // Limpa a flag TxDone
            lora_set_mode(MODE_STDBY); // Volta para Standby após enviar
            printf("Pacote enviado com sucesso!\n");
            return true; // Sucesso
        }
        busy_wait_ms_local(1); // Espera 1ms antes de verificar de novo
        timeout_cnt--;
    }

    // Se saiu do loop, ocorreu timeout
    printf("Erro: Timeout de TX! O radio foi resetado para Standby.\n");
    lora_set_mode(MODE_STDBY); // Tenta voltar para Standby para abortar TX
    return false; // Falha (timeout)
}