#ifndef AHT10_H_
#define AHT10_H_

#include <stdint.h>
#include <stdbool.h>

// ============================================
// === Struct de Dados ===
// ============================================
/**
 * @brief Estrutura para armazenar dados do sensor.
 * Valores são multiplicados por 100 para evitar ponto flutuante.
 */
typedef struct {
    int16_t temperatura; // Temperatura * 100
    int16_t umidade;     // Umidade * 100
} dados;


// ============================================
// === Protótipos Públicos ===
// ============================================

/**
 * @brief Inicializa o driver I2C bitbang.
 * Deve ser chamada antes de qualquer outra função I2C ou AHT10.
 */
void i2c_init(void);

/**
 * @brief Varre o barramento I2C e imprime endereços de dispositivos encontrados.
 */
void i2c_scan(void);

/**
 * @brief Inicializa o sensor AHT10.
 * @return 0 em sucesso, -1 em falha.
 */
int aht10_init(void);

/**
 * @brief Lê o sensor AHT10 e imprime os valores formatados (Debug).
 */
void aht10_read(void);

/**
 * @brief Obtém os dados de temperatura e umidade do AHT10.
 * @param d Ponteiro para a struct 'dados' onde os resultados serão armazenados.
 * @return true em sucesso, false em falha.
 */
bool aht10_get_data(dados *d);

#endif // AHT10_H_