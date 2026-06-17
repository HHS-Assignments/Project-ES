#pragma once

#include "stm32f3xx_hal.h"

extern UART_HandleTypeDef huart2;

// Wacht x milliseconden zonder HAL_Delay (interrupt-veilig)
void Safe_Delay(uint32_t ms) {
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < ms);
}

// Zet een GPIO pin hoog
void ZetAan(GPIO_TypeDef* GPIO, uint16_t pin) {
    HAL_GPIO_WritePin(GPIO, pin, GPIO_PIN_SET);
}

// Zet een GPIO pin laag
void ZetUit(GPIO_TypeDef* GPIO, uint16_t pin) {
    HAL_GPIO_WritePin(GPIO, pin, GPIO_PIN_RESET);
}

// Stuurt een string over UART2
void UART_Print(const char* str) {
    while (*str) {
        HAL_UART_Transmit(&huart2, (uint8_t*)str, 1, HAL_MAX_DELAY);
        str++;
    }
    HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);
}


