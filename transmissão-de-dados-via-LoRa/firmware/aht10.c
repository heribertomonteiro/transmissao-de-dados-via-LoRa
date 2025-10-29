#include "aht10.h"
#include <stdio.h>
#include <generated/csr.h>
#include <system.h> // Para busy_wait_us

// ============================================
// === Utils de tempo (Cópia local) ===
// ============================================
// Esta função é necessária pelo driver I2C e AHT10
static void busy_wait_ms(unsigned int ms) {
    for (unsigned int i = 0; i < ms; ++i) {
#ifdef CSR_TIMER0_BASE
        busy_wait_us(1000);
#else
        for(volatile int j = 0; j < 1000; j++);
#endif
    }
}

// ============================================
// === I2C Driver (Bitbang com CSR) ===
// ============================================

static uint32_t i2c_w_reg = 0;
static void i2c_delay(void) { busy_wait_us(5); }

static void i2c_set_scl(int val) {
    if (val) i2c_w_reg |= (1 << CSR_I2C_W_SCL_OFFSET);
    else     i2c_w_reg &= ~(1 << CSR_I2C_W_SCL_OFFSET);
    i2c_w_write(i2c_w_reg);
}

static void i2c_set_sda(int val) {
    if (val) i2c_w_reg |= (1 << CSR_I2C_W_SDA_OFFSET);
    else     i2c_w_reg &= ~(1 << CSR_I2C_W_SDA_OFFSET);
    i2c_w_write(i2c_w_reg);
}

static void i2c_set_oe(int val) {
    if (val) i2c_w_reg |= (1 << CSR_I2C_W_OE_OFFSET);
    else     i2c_w_reg &= ~(1 << CSR_I2C_W_OE_OFFSET);
    i2c_w_write(i2c_w_reg);
}

static int i2c_read_sda(void) {
    return (i2c_r_read() & (1 << CSR_I2C_R_SDA_OFFSET)) != 0;
}

// --- Funções Públicas (do aht10.h) ---
void i2c_init(void) {
    i2c_set_oe(1); i2c_set_scl(1); i2c_set_sda(1);
    busy_wait_ms(1);
}

// --- Funções Internas (static) ---
static void i2c_start(void) {
    i2c_set_sda(1); i2c_set_oe(1); i2c_set_scl(1); i2c_delay();
    i2c_set_sda(0); i2c_delay();
    i2c_set_scl(0); i2c_delay();
}

static void i2c_stop(void) {
    i2c_set_sda(0); i2c_set_oe(1); i2c_set_scl(0); i2c_delay();
    i2c_set_scl(1); i2c_delay();
    i2c_set_sda(1); i2c_delay();
}

static bool i2c_write_byte(uint8_t byte) {
    int i; bool ack;
    i2c_set_oe(1);
    for (i = 0; i < 8; i++) {
        i2c_set_sda((byte & 0x80) != 0); i2c_delay();
        i2c_set_scl(1); i2c_delay();
        i2c_set_scl(0); i2c_delay();
        byte <<= 1;
    }
    i2c_set_oe(0); i2c_set_sda(1); i2c_delay();
    i2c_set_scl(1); i2c_delay();
    ack = !i2c_read_sda();
    i2c_set_scl(0); i2c_delay();
    return ack;
}

static uint8_t i2c_read_byte(bool send_ack) {
    int i; uint8_t byte = 0;
    i2c_set_oe(0); i2c_set_sda(1); i2c_delay();
    for (i = 0; i < 8; i++) {
        byte <<= 1;
        i2c_set_scl(1); i2c_delay();
        if (i2c_read_sda()) byte |= 1;
        i2c_set_scl(0); i2c_delay();
    }
    i2c_set_oe(1); i2c_set_sda(!send_ack); i2c_delay();
    i2c_set_scl(1); i2c_delay();
    i2c_set_scl(0); i2c_delay();
    return byte;
}

// --- Função Pública (do aht10.h) ---
void i2c_scan(void) {
    printf("Escaneando barramento I2C...\n");
    for (uint8_t addr = 1; addr < 128; addr++) {
        i2c_start();
        if (i2c_write_byte(addr << 1 | 0)) {
            printf("  Dispositivo encontrado em 0x%02X\n", addr);
        }
        i2c_stop();
        busy_wait_us(100);
    }
    printf("Scan completo.\n");
}


// ============================================
// === Sensor AHT10 (Usa o driver acima) ===
// ============================================
#define AHT10_I2C_ADDR 0x38

// --- Funções Públicas (do aht10.h) ---

int aht10_init(void) {
    i2c_start();
    if (!i2c_write_byte(AHT10_I2C_ADDR << 1 | 0)) { i2c_stop(); return -1; }
    if (!i2c_write_byte(0xE1)) { i2c_stop(); return -1; }
    if (!i2c_write_byte(0x08)) { i2c_stop(); return -1; }
    if (!i2c_write_byte(0x00)) { i2c_stop(); return -1; }
    i2c_stop();
    busy_wait_ms(100);
    return 0;
}

bool aht10_get_data(dados *d) {
    uint8_t data[6];
    uint32_t raw_hum, raw_temp;
    
    // 1. Dispara a medição
    i2c_start();
    if (!i2c_write_byte(AHT10_I2C_ADDR << 1 | 0)) { i2c_stop(); return false; } // Escrita
    if (!i2c_write_byte(0xAC)) { i2c_stop(); return false; }
    if (!i2c_write_byte(0x33)) { i2c_stop(); return false; }
    if (!i2c_write_byte(0x00)) { i2c_stop(); return false; }
    i2c_stop();

    // 2. Espera pela medição
    busy_wait_ms(80);

    // 3. Lê os 6 bytes de dados
    i2c_start();
    if (!i2c_write_byte(AHT10_I2C_ADDR << 1 | 1)) { i2c_stop(); return false; } // Leitura
    data[0] = i2c_read_byte(true);
    data[1] = i2c_read_byte(true);
    data[2] = i2c_read_byte(true);
    data[3] = i2c_read_byte(true);
    data[4] = i2c_read_byte(true);
    data[5] = i2c_read_byte(false); // NACK
    i2c_stop();

    // 4. Verifica o bit de "busy"
    if (data[0] & 0x80) {
        printf("Erro: AHT10 ainda ocupado.\n");
        return false;
    }

    // 5. Calcula os valores
    raw_hum = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
    raw_temp = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];

    // Umidade = (raw_hum * 10000) / 2^20 (para *100)
    d->umidade = (int16_t)(((uint64_t)raw_hum * 10000) / 0x100000);

    // Temperatura = (raw_temp * 20000) / 2^20 - 5000 (para *100)
    d->temperatura = (int16_t)((((uint64_t)raw_temp * 20000) / 0x100000) - 5000);
    
    return true;
}

void aht10_read(void) {
    dados my_data;
    printf("Lendo AHT10 (modo debug)...\n");
    if (aht10_get_data(&my_data)) {
        // Imprime os valores com duas casas decimais (dividindo por 100)
        printf("Umidade: %d.%02d %%\n",
            my_data.umidade / 100, my_data.umidade % 100);
        printf("Temperatura: %d.%02d C\n",
            my_data.temperatura / 100, (my_data.temperatura > 0 ? my_data.temperatura : -my_data.temperatura) % 100);
    } else {
        printf("Falha ao ler AHT10.\n");
    }
}