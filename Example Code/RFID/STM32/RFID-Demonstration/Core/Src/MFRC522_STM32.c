/**
 * MFRC522_STM32.cpp - MFRC522 RFID library ported to STM32 HAL
 *
 * All Arduino/AVR specifics have been replaced:
 *   SPI.*              → HAL_SPI_Transmit / HAL_SPI_TransmitReceive
 *   digitalWrite/Read  → HAL_GPIO_WritePin / HAL_GPIO_ReadPin
 *   pinMode            → HAL_GPIO_Init  (via SetPinAsInput/Output helpers)
 *   delay / millis     → HAL_Delay / HAL_GetTick
 *   delayMicroseconds  → DWT cycle-counter busy-wait  (InitDWT called once)
 *   Serial.print       → printf  (retarget _write() to your UART or SWO)
 *   F() / PROGMEM      → plain const arrays / const char* (not needed on STM32)
 *   pgm_read_byte      → direct array access
 *   yield()            → removed  (or replace with osDelay(0) for RTOS)
 */

#include "MFRC522_STM32.h"
#include <string.h>   // memcpy
#include <stdio.h>    // printf  (retarget to your debug channel)

/* ─────────────────────────────────────────────────────────────────────────
 *  Firmware self-test reference data
 *  (Originally PROGMEM arrays; on STM32 const data lives in flash already.)
 * ───────────────────────────────────────────────────────────────────────── */

// Version 0.0 (0x90)
static const uint8_t MFRC522_firmware_referenceV0_0[64] = {
    0x00, 0x87, 0x98, 0x0f, 0x49, 0xFF, 0x07, 0x19,
    0xBF, 0x22, 0x30, 0x49, 0x59, 0x63, 0xAD, 0xCA,
    0x7F, 0xE3, 0x4E, 0x03, 0x5C, 0x4E, 0x49, 0x50,
    0x47, 0x9A, 0x37, 0x61, 0xE7, 0xE2, 0xC6, 0x2E,
    0x75, 0x5A, 0xED, 0x04, 0x3D, 0x02, 0x4B, 0x78,
    0x32, 0xFF, 0x58, 0x3B, 0x7C, 0xE9, 0x00, 0x94,
    0xB4, 0x4A, 0x59, 0x5B, 0xFD, 0xC9, 0x29, 0xDF,
    0x35, 0x96, 0x98, 0x9E, 0x4F, 0x30, 0x32, 0x8D
};

// Version 1.0 (0x91)
static const uint8_t MFRC522_firmware_referenceV1_0[64] = {
    0x00, 0xC6, 0x37, 0xD5, 0x32, 0xB7, 0x57, 0x5C,
    0xC2, 0xD8, 0x7C, 0x4D, 0xD9, 0x70, 0xC7, 0x73,
    0x10, 0xE6, 0xD2, 0xAA, 0x5E, 0xA1, 0x3E, 0x5A,
    0x14, 0xAF, 0x30, 0x61, 0xC9, 0x70, 0xDB, 0x2E,
    0x64, 0x22, 0x72, 0xB5, 0xBD, 0x65, 0xF4, 0xEC,
    0x22, 0xBC, 0xD3, 0x72, 0x35, 0xCD, 0xAA, 0x41,
    0x1F, 0xA7, 0xF3, 0x53, 0x14, 0xDE, 0x7E, 0x02,
    0xD9, 0x0F, 0xB5, 0x5E, 0x25, 0x1D, 0x29, 0x79
};

// Version 2.0 (0x92)
static const uint8_t MFRC522_firmware_referenceV2_0[64] = {
    0x00, 0xEB, 0x66, 0xBA, 0x57, 0xBF, 0x23, 0x95,
    0xD0, 0xE3, 0x0D, 0x3D, 0x27, 0x89, 0x5C, 0xDE,
    0x9D, 0x3B, 0xA7, 0x00, 0x21, 0x5B, 0x89, 0x82,
    0x51, 0x3A, 0xEB, 0x02, 0x0C, 0xA5, 0x00, 0x49,
    0x7C, 0x84, 0x4D, 0xB3, 0xCC, 0xD2, 0x1B, 0x81,
    0x5D, 0x48, 0x76, 0xD5, 0x71, 0x61, 0x21, 0xA9,
    0x86, 0x96, 0x83, 0x38, 0xCF, 0x9D, 0x5B, 0x6D,
    0xDC, 0x15, 0xBA, 0x3E, 0x7D, 0x95, 0x3B, 0x2F
};

// Fudan Semiconductor FM17522 clone (0x88)
static const uint8_t FM17522_firmware_reference[64] = {
    0x00, 0xD6, 0x78, 0x8C, 0xE2, 0xAA, 0x0C, 0x18,
    0x2A, 0xB8, 0x7A, 0x7F, 0xD3, 0x6A, 0xCF, 0x0B,
    0xB1, 0x37, 0x63, 0x4B, 0x69, 0xAE, 0x91, 0xC7,
    0xC3, 0x97, 0xAE, 0x77, 0xF4, 0x37, 0xD7, 0x9B,
    0x7C, 0xF5, 0x3C, 0x11, 0x8F, 0x15, 0xC3, 0xD7,
    0xC1, 0x5B, 0x00, 0x2A, 0xD0, 0x75, 0xDE, 0x9E,
    0x51, 0x64, 0xAB, 0x3E, 0xE9, 0x15, 0xB5, 0xAB,
    0x56, 0x9A, 0x98, 0x82, 0x26, 0xEA, 0x2A, 0x62
};


/* ═══════════════════════════════════════════════════════════════════════════
 *  Constructors
 * ═════════════════════════════════════════════════════════════════════════ */

MFRC522::MFRC522(SPI_HandleTypeDef *hspi,
                 GPIO_TypeDef *csPort,  uint16_t csPin,
                 GPIO_TypeDef *rstPort, uint16_t rstPin)
    : _hspi(hspi),
      _csPort(csPort), _csPin(csPin),
      _rstPort(rstPort), _rstPin(rstPin)
{
}

MFRC522::MFRC522(SPI_HandleTypeDef *hspi,
                 GPIO_TypeDef *csPort, uint16_t csPin)
    : _hspi(hspi),
      _csPort(csPort), _csPin(csPin),
      _rstPort(nullptr), _rstPin(0)
{
}


/* ═══════════════════════════════════════════════════════════════════════════
 *  Private low-level helpers
 * ═════════════════════════════════════════════════════════════════════════ */

/**
 * Enable the DWT cycle counter (used by DelayMicroseconds).
 * Safe to call multiple times.
 */
void MFRC522::InitDWT() {
    /* CoreDebug and DWT are part of the ARM Cortex-M core debug block.
     * They are available on Cortex-M3/M4/M7; on M0/M0+ DWT may be absent,
     * in which case DelayMicroseconds falls back to a dumb loop.         */
#if defined(DWT)
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
#if defined(DWT_LAR_Present) || defined(__CORTEX_M) && (__CORTEX_M == 7U)
    DWT->LAR = 0xC5ACCE55U;   // Unlock DWT on Cortex-M7
#endif
    DWT->CYCCNT = 0U;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
#endif
}

/**
 * Busy-wait for the specified number of microseconds.
 * Uses the DWT cycle counter when available; falls back to a loop.
 */
void MFRC522::DelayMicroseconds(uint32_t us) {
#if defined(DWT)
    uint32_t start  = DWT->CYCCNT;
    uint32_t cycles = us * (HAL_RCC_GetHCLKFreq() / 1000000UL);
    while ((DWT->CYCCNT - start) < cycles) { /* busy-wait */ }
#else
    /* Fallback: rough loop (calibrate the divisor for your clock speed). */
    volatile uint32_t count = us * (SystemCoreClock / 4000000UL);
    while (count--) { __NOP(); }
#endif
}

/** Reconfigure a GPIO pin as push-pull output (needed for RST toggling). */
void MFRC522::SetPinAsOutput(GPIO_TypeDef *port, uint16_t pin) {
    GPIO_InitTypeDef cfg = {};
    cfg.Pin   = pin;
    cfg.Mode  = GPIO_MODE_OUTPUT_PP;
    cfg.Pull  = GPIO_NOPULL;
    cfg.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &cfg);
}

/** Reconfigure a GPIO pin as floating input (needed for RST power-down check). */
void MFRC522::SetPinAsInput(GPIO_TypeDef *port, uint16_t pin) {
    GPIO_InitTypeDef cfg = {};
    cfg.Pin  = pin;
    cfg.Mode = GPIO_MODE_INPUT;
    cfg.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(port, &cfg);
}


/* ═══════════════════════════════════════════════════════════════════════════
 *  Basic SPI interface  (section 8.1.2 of the MFRC522 datasheet)
 * ═════════════════════════════════════════════════════════════════════════ */

/**
 * Write a single byte to an MFRC522 register.
 * Address byte: MSB=0 (write), LSB unused → already embedded in PCD_Register enum.
 */
void MFRC522::PCD_WriteRegister(PCD_Register reg, uint8_t value) {
    uint8_t buf[2] = { static_cast<uint8_t>(reg), value };
    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_RESET);   // CS low  → select
    HAL_SPI_Transmit(_hspi, buf, 2, MFRC522_SPITIMEOUT);
    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_SET);     // CS high → release
}

/**
 * Write multiple bytes to an MFRC522 register.
 */
void MFRC522::PCD_WriteRegister(PCD_Register reg, uint8_t count, uint8_t *values) {
    uint8_t addr = static_cast<uint8_t>(reg);
    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(_hspi, &addr,  1,     MFRC522_SPITIMEOUT);
    HAL_SPI_Transmit(_hspi, values, count, MFRC522_SPITIMEOUT);
    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_SET);
}

/**
 * Read a single byte from an MFRC522 register.
 * Address byte: MSB=1 (read).
 */
uint8_t MFRC522::PCD_ReadRegister(PCD_Register reg) {
    uint8_t addr  = static_cast<uint8_t>(0x80 | reg);
    uint8_t dummy = 0x00;
    uint8_t value = 0x00;

    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_RESET);
    // Send address, ignore received byte (MISO during address phase is don't-care)
    HAL_SPI_TransmitReceive(_hspi, &addr,  &value, 1, MFRC522_SPITIMEOUT);
    // Send dummy 0x00; received byte is the register value
    HAL_SPI_TransmitReceive(_hspi, &dummy, &value, 1, MFRC522_SPITIMEOUT);
    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_SET);

    return value;
}

/**
 * Read multiple bytes from an MFRC522 register.
 *
 * The MFRC522 uses a "streaming" SPI read: you keep sending the same address
 * byte to clock out subsequent bytes, then send 0x00 for the final byte.
 *
 * @param rxAlign  First valid bit position in values[0] (0 = normal).
 */
void MFRC522::PCD_ReadRegister(PCD_Register reg,
                                uint8_t  count,
                                uint8_t *values,
                                uint8_t  rxAlign) {
    if (count == 0) return;

    uint8_t addr  = static_cast<uint8_t>(0x80 | reg);
    uint8_t dummy = 0x00;
    uint8_t index = 0;
    uint8_t rxByte;

    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_RESET);

    count--;   // one byte will be read outside the loop

    // Send address (received byte during this transfer is don't-care)
    HAL_SPI_TransmitReceive(_hspi, &addr, &rxByte, 1, MFRC522_SPITIMEOUT);

    if (rxAlign) {
        // Merge received bits into values[0] at positions [rxAlign..7]
        uint8_t mask = (0xFF << rxAlign) & 0xFF;
        HAL_SPI_TransmitReceive(_hspi, &addr, &rxByte, 1, MFRC522_SPITIMEOUT);
        values[0] = (values[0] & ~mask) | (rxByte & mask);
        index++;
    }

    while (index < count) {
        HAL_SPI_TransmitReceive(_hspi, &addr, &rxByte, 1, MFRC522_SPITIMEOUT);
        values[index] = rxByte;
        index++;
    }

    // Final byte: send 0x00 to stop reading
    HAL_SPI_TransmitReceive(_hspi, &dummy, &rxByte, 1, MFRC522_SPITIMEOUT);
    values[index] = rxByte;

    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_SET);
}

void MFRC522::PCD_SetRegisterBitMask(PCD_Register reg, uint8_t mask) {
    PCD_WriteRegister(reg, PCD_ReadRegister(reg) | mask);
}

void MFRC522::PCD_ClearRegisterBitMask(PCD_Register reg, uint8_t mask) {
    PCD_WriteRegister(reg, PCD_ReadRegister(reg) & (~mask));
}

/**
 * Use the MFRC522's CRC coprocessor to compute CRC_A.
 * Timeout after ~89 ms.
 */
MFRC522::StatusCode MFRC522::PCD_CalculateCRC(uint8_t *data,
                                               uint8_t  length,
                                               uint8_t *result) {
    PCD_WriteRegister(CommandReg,   PCD_Idle);        // stop any running command
    PCD_WriteRegister(DivIrqReg,    0x04);            // clear CRCIRq
    PCD_WriteRegister(FIFOLevelReg, 0x80);            // flush FIFO
    PCD_WriteRegister(FIFODataReg,  length, data);    // write data to FIFO
    PCD_WriteRegister(CommandReg,   PCD_CalcCRC);     // start calculation

    uint32_t deadline = HAL_GetTick() + 89UL;
    do {
        uint8_t n = PCD_ReadRegister(DivIrqReg);
        if (n & 0x04) {   // CRCIRq set → done
            PCD_WriteRegister(CommandReg, PCD_Idle);
            result[0] = PCD_ReadRegister(CRCResultRegL);
            result[1] = PCD_ReadRegister(CRCResultRegH);
            return STATUS_OK;
        }
    } while (HAL_GetTick() < deadline);

    return STATUS_TIMEOUT;
}


/* ═══════════════════════════════════════════════════════════════════════════
 *  PCD initialisation and control
 * ═════════════════════════════════════════════════════════════════════════ */

/**
 * Initialise the MFRC522.
 * Performs a hard reset via the NRSTPD pin if connected, otherwise soft reset.
 * Call once after power-on (or after PCD_SoftPowerDown).
 */
void MFRC522::PCD_Init() {
    InitDWT();   // enable microsecond delay counter

    /* Deselect chip (CS high) */
    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_SET);

    bool hardReset = false;

    if (_rstPort != nullptr && _rstPin != 0) {
        /* Read RST as input to check power-down state */
        SetPinAsInput(_rstPort, _rstPin);

        if (HAL_GPIO_ReadPin(_rstPort, _rstPin) == GPIO_PIN_RESET) {
            /* Chip is in power-down → toggle RST to trigger hard reset */
            SetPinAsOutput(_rstPort, _rstPin);
            HAL_GPIO_WritePin(_rstPort, _rstPin, GPIO_PIN_RESET);
            DelayMicroseconds(2);                             // ≥ 100 ns
            HAL_GPIO_WritePin(_rstPort, _rstPin, GPIO_PIN_SET);
            HAL_Delay(50);                                    // oscillator start-up
            hardReset = true;
        } else {
            /* Pin was already high: keep it as output and drive high */
            SetPinAsOutput(_rstPort, _rstPin);
            HAL_GPIO_WritePin(_rstPort, _rstPin, GPIO_PIN_SET);
        }
    }

    if (!hardReset) {
        PCD_Reset();
    }

    /* Restore default baud-rate and modulation width */
    PCD_WriteRegister(TxModeReg,   0x00);
    PCD_WriteRegister(RxModeReg,   0x00);
    PCD_WriteRegister(ModWidthReg, 0x26);

    /* Timer: f_timer = 13.56 MHz / (2 * 169 + 1) ≈ 40 kHz → period 25 µs
     *        Reload  = 1000 → timeout = 25 ms                              */
    PCD_WriteRegister(TModeReg,      0x80);   // TAuto=1
    PCD_WriteRegister(TPrescalerReg, 0xA9);   // TPreScaler = 169
    PCD_WriteRegister(TReloadRegH,   0x03);
    PCD_WriteRegister(TReloadRegL,   0xE8);   // reload 0x03E8 = 1000

    PCD_WriteRegister(TxASKReg, 0x40);   // force 100% ASK
    PCD_WriteRegister(ModeReg,  0x3D);   // CRC preset 0x6363 (ISO 14443-3)

    PCD_AntennaOn();
}

/**
 * Perform a soft reset and wait for the chip to be ready (max ~150 ms).
 */
void MFRC522::PCD_Reset() {
    PCD_WriteRegister(CommandReg, PCD_SoftReset);

    uint8_t count = 0;
    do {
        HAL_Delay(50);
    } while ((PCD_ReadRegister(CommandReg) & (1 << 4)) && (++count < 3));
}

void MFRC522::PCD_AntennaOn() {
    uint8_t value = PCD_ReadRegister(TxControlReg);
    if ((value & 0x03) != 0x03) {
        PCD_WriteRegister(TxControlReg, value | 0x03);
    }
}

void MFRC522::PCD_AntennaOff() {
    PCD_ClearRegisterBitMask(TxControlReg, 0x03);
}

uint8_t MFRC522::PCD_GetAntennaGain() {
    return PCD_ReadRegister(RFCfgReg) & (0x07 << 4);
}

void MFRC522::PCD_SetAntennaGain(uint8_t mask) {
    if (PCD_GetAntennaGain() != mask) {
        PCD_ClearRegisterBitMask(RFCfgReg, (0x07 << 4));
        PCD_SetRegisterBitMask  (RFCfgReg, mask & (0x07 << 4));
    }
}

/**
 * Self-test (section 16.1.1 of the datasheet).
 * @return true if the firmware signature matches a known reference.
 */
bool MFRC522::PCD_PerformSelfTest() {
    PCD_Reset();

    // Write 25 × 0x00 to the internal buffer via FIFO
    uint8_t ZEROES[25] = {};
    PCD_WriteRegister(FIFOLevelReg, 0x80);
    PCD_WriteRegister(FIFODataReg,  25, ZEROES);
    PCD_WriteRegister(CommandReg,   PCD_Mem);

    // Enable self-test
    PCD_WriteRegister(AutoTestReg, 0x09);
    PCD_WriteRegister(FIFODataReg, 0x00);
    PCD_WriteRegister(CommandReg,  PCD_CalcCRC);

    // Wait until FIFO contains 64 bytes of result
    for (uint16_t i = 0; i < 0xFF; i++) {
        if (PCD_ReadRegister(FIFOLevelReg) >= 64) break;
    }
    PCD_WriteRegister(CommandReg, PCD_Idle);

    // Read result
    uint8_t result[64];
    PCD_ReadRegister(FIFODataReg, 64, result, 0);

    // Disable self-test
    PCD_WriteRegister(AutoTestReg, 0x00);

    // Determine firmware version and select reference
    uint8_t version = PCD_ReadRegister(VersionReg);
    const uint8_t *reference;
    switch (version) {
        case 0x88: reference = FM17522_firmware_reference;       break;
        case 0x90: reference = MFRC522_firmware_referenceV0_0;   break;
        case 0x91: reference = MFRC522_firmware_referenceV1_0;   break;
        case 0x92: reference = MFRC522_firmware_referenceV2_0;   break;
        default:   return false;
    }

    for (uint8_t i = 0; i < 64; i++) {
        if (result[i] != reference[i]) return false;
    }

    PCD_Init();   // re-initialise after self-test
    return true;
}


/* ═══════════════════════════════════════════════════════════════════════════
 *  Power control
 * ═════════════════════════════════════════════════════════════════════════ */

void MFRC522::PCD_SoftPowerDown() {
    uint8_t val = PCD_ReadRegister(CommandReg);
    val |= (1 << 4);   // set PowerDown bit
    PCD_WriteRegister(CommandReg, val);
}

void MFRC522::PCD_SoftPowerUp() {
    uint8_t val = PCD_ReadRegister(CommandReg);
    val &= ~(1 << 4);  // clear PowerDown bit
    PCD_WriteRegister(CommandReg, val);

    uint32_t timeout = HAL_GetTick() + 500UL;
    while (HAL_GetTick() <= timeout) {
        val = PCD_ReadRegister(CommandReg);
        if (!(val & (1 << 4))) break;   // wake-up complete
    }
}


/* ═══════════════════════════════════════════════════════════════════════════
 *  PICC communication
 * ═════════════════════════════════════════════════════════════════════════ */

MFRC522::StatusCode MFRC522::PCD_TransceiveData(
        uint8_t *sendData, uint8_t sendLen,
        uint8_t *backData, uint8_t *backLen,
        uint8_t *validBits, uint8_t rxAlign, bool checkCRC) {
    uint8_t waitIRq = 0x30;   // RxIRq | IdleIRq
    return PCD_CommunicateWithPICC(PCD_Transceive, waitIRq,
                                   sendData, sendLen,
                                   backData, backLen,
                                   validBits, rxAlign, checkCRC);
}

/**
 * Core function: write to FIFO, execute a command, wait for completion,
 * read back from FIFO.
 */
MFRC522::StatusCode MFRC522::PCD_CommunicateWithPICC(
        uint8_t  command,   uint8_t  waitIRq,
        uint8_t *sendData,  uint8_t  sendLen,
        uint8_t *backData,  uint8_t *backLen,
        uint8_t *validBits, uint8_t  rxAlign,
        bool     checkCRC) {

    uint8_t txLastBits  = validBits ? *validBits : 0;
    uint8_t bitFraming  = (uint8_t)((rxAlign << 4) + txLastBits);

    PCD_WriteRegister(CommandReg,    PCD_Idle);           // abort previous command
    PCD_WriteRegister(ComIrqReg,     0x7F);               // clear interrupt flags
    PCD_WriteRegister(FIFOLevelReg,  0x80);               // flush FIFO
    PCD_WriteRegister(FIFODataReg,   sendLen, sendData);  // load data
    PCD_WriteRegister(BitFramingReg, bitFraming);
    PCD_WriteRegister(CommandReg,    command);

    if (command == PCD_Transceive) {
        PCD_SetRegisterBitMask(BitFramingReg, 0x80);      // StartSend=1
    }

    // Wait for completion (≤ 36 ms)
    uint32_t deadline  = HAL_GetTick() + 36UL;
    bool     completed = false;

    do {
        uint8_t n = PCD_ReadRegister(ComIrqReg);
        if (n & waitIRq) {
            completed = true;
            break;
        }
        if (n & 0x01) {              // TimerIRq → nothing received in 25 ms
            return STATUS_TIMEOUT;
        }
    } while (HAL_GetTick() < deadline);

    if (!completed) return STATUS_TIMEOUT;

    // Check for protocol / parity / buffer-overflow errors
    uint8_t errorRegValue = PCD_ReadRegister(ErrorReg);
    if (errorRegValue & 0x13) {      // BufferOvfl | ParityErr | ProtocolErr
        return STATUS_ERROR;
    }

    uint8_t _validBits = 0;

    if (backData && backLen) {
        uint8_t n = PCD_ReadRegister(FIFOLevelReg);
        if (n > *backLen) return STATUS_NO_ROOM;
        *backLen = n;
        PCD_ReadRegister(FIFODataReg, n, backData, rxAlign);
        _validBits = PCD_ReadRegister(ControlReg) & 0x07;
        if (validBits) *validBits = _validBits;
    }

    if (errorRegValue & 0x08) return STATUS_COLLISION;   // CollErr

    if (backData && backLen && checkCRC) {
        if (*backLen == 1 && _validBits == 4) return STATUS_MIFARE_NACK;
        if (*backLen < 2 || _validBits != 0)  return STATUS_CRC_WRONG;

        uint8_t controlBuffer[2];
        StatusCode status = PCD_CalculateCRC(backData, *backLen - 2, controlBuffer);
        if (status != STATUS_OK) return status;
        if ((backData[*backLen - 2] != controlBuffer[0]) ||
            (backData[*backLen - 1] != controlBuffer[1])) {
            return STATUS_CRC_WRONG;
        }
    }

    return STATUS_OK;
}

MFRC522::StatusCode MFRC522::PICC_RequestA(uint8_t *bufferATQA,
                                            uint8_t *bufferSize) {
    return PICC_REQA_or_WUPA(PICC_CMD_REQA, bufferATQA, bufferSize);
}

MFRC522::StatusCode MFRC522::PICC_WakeupA(uint8_t *bufferATQA,
                                           uint8_t *bufferSize) {
    return PICC_REQA_or_WUPA(PICC_CMD_WUPA, bufferATQA, bufferSize);
}

MFRC522::StatusCode MFRC522::PICC_REQA_or_WUPA(uint8_t  command,
                                                 uint8_t *bufferATQA,
                                                 uint8_t *bufferSize) {
    if (bufferATQA == nullptr || *bufferSize < 2) return STATUS_NO_ROOM;

    PCD_ClearRegisterBitMask(CollReg, 0x80);   // ValuesAfterColl=1

    uint8_t validBits = 7;   // short frame: 7 bits only
    StatusCode status = PCD_TransceiveData(&command, 1,
                                           bufferATQA, bufferSize,
                                           &validBits);
    if (status != STATUS_OK) return status;
    if (*bufferSize != 2 || validBits != 0) return STATUS_ERROR;
    return STATUS_OK;
}

/**
 * Anti-collision and SELECT sequence (ISO 14443-3).
 * Supports single (4 B), double (7 B) and triple (10 B) UIDs.
 */
MFRC522::StatusCode MFRC522::PICC_Select(Uid *uid, uint8_t validBits) {
    bool uidComplete, selectDone, useCascadeTag;
    uint8_t cascadeLevel = 1;
    StatusCode result;
    uint8_t count, checkBit, index, uidIndex;
    int8_t  currentLevelKnownBits;
    uint8_t buffer[9];       // SELECT/ANTICOLLISION frame + CRC
    uint8_t bufferUsed;
    uint8_t rxAlign, txLastBits;
    uint8_t *responseBuffer;
    uint8_t  responseLength;

    if (validBits > 80) return STATUS_INVALID;

    PCD_ClearRegisterBitMask(CollReg, 0x80);

    uidComplete = false;
    while (!uidComplete) {
        switch (cascadeLevel) {
            case 1:
                buffer[0] = PICC_CMD_SEL_CL1;
                uidIndex = 0;
                useCascadeTag = validBits && uid->size > 4;
                break;
            case 2:
                buffer[0] = PICC_CMD_SEL_CL2;
                uidIndex = 3;
                useCascadeTag = validBits && uid->size > 7;
                break;
            case 3:
                buffer[0] = PICC_CMD_SEL_CL3;
                uidIndex = 6;
                useCascadeTag = false;
                break;
            default:
                return STATUS_INTERNAL_ERROR;
        }

        currentLevelKnownBits = (int8_t)(validBits - (8 * uidIndex));
        if (currentLevelKnownBits < 0) currentLevelKnownBits = 0;

        index = 2;
        if (useCascadeTag) buffer[index++] = PICC_CMD_CT;

        uint8_t bytesToCopy = (uint8_t)(currentLevelKnownBits / 8 +
                              (currentLevelKnownBits % 8 ? 1 : 0));
        if (bytesToCopy) {
            uint8_t maxBytes = useCascadeTag ? 3 : 4;
            if (bytesToCopy > maxBytes) bytesToCopy = maxBytes;
            for (count = 0; count < bytesToCopy; count++) {
                buffer[index++] = uid->uidByte[uidIndex + count];
            }
        }
        if (useCascadeTag) currentLevelKnownBits += 8;

        selectDone = false;
        while (!selectDone) {
            if (currentLevelKnownBits >= 32) {
                buffer[1] = 0x70;   // NVB: 7 complete bytes
                buffer[6] = buffer[2] ^ buffer[3] ^ buffer[4] ^ buffer[5];   // BCC
                result = PCD_CalculateCRC(buffer, 7, &buffer[7]);
                if (result != STATUS_OK) return result;
                txLastBits     = 0;
                bufferUsed     = 9;
                responseBuffer = &buffer[6];
                responseLength = 3;
            } else {
                txLastBits     = (uint8_t)(currentLevelKnownBits % 8);
                count          = (uint8_t)(currentLevelKnownBits / 8);
                index          = (uint8_t)(2 + count);
                buffer[1]      = (uint8_t)((index << 4) + txLastBits);
                bufferUsed     = (uint8_t)(index + (txLastBits ? 1 : 0));
                responseBuffer = &buffer[index];
                responseLength = (uint8_t)(sizeof(buffer) - index);
            }

            rxAlign = txLastBits;
            PCD_WriteRegister(BitFramingReg,
                              (uint8_t)((rxAlign << 4) + txLastBits));

            result = PCD_TransceiveData(buffer, bufferUsed,
                                        responseBuffer, &responseLength,
                                        &txLastBits, rxAlign);

            if (result == STATUS_COLLISION) {
                uint8_t valueOfCollReg = PCD_ReadRegister(CollReg);
                if (valueOfCollReg & 0x20) return STATUS_COLLISION;
                uint8_t collisionPos = valueOfCollReg & 0x1F;
                if (collisionPos == 0) collisionPos = 32;
                if ((int8_t)collisionPos <= currentLevelKnownBits) {
                    return STATUS_INTERNAL_ERROR;
                }
                currentLevelKnownBits = (int8_t)collisionPos;
                count    = (uint8_t)(currentLevelKnownBits % 8);
                checkBit = (uint8_t)((currentLevelKnownBits - 1) % 8);
                index    = (uint8_t)(1 + currentLevelKnownBits / 8 + (count ? 1 : 0));
                buffer[index] |= (1 << checkBit);
            } else if (result != STATUS_OK) {
                return result;
            } else {
                if (currentLevelKnownBits >= 32) {
                    selectDone = true;
                } else {
                    currentLevelKnownBits = 32;
                }
            }
        }

        // Copy UID bytes from buffer to uid struct
        index      = (buffer[2] == PICC_CMD_CT) ? 3 : 2;
        bytesToCopy = (buffer[2] == PICC_CMD_CT) ? 3 : 4;
        for (count = 0; count < bytesToCopy; count++) {
            uid->uidByte[uidIndex + count] = buffer[index++];
        }

        // Verify SAK
        if (responseLength != 3 || txLastBits != 0) return STATUS_ERROR;
        result = PCD_CalculateCRC(responseBuffer, 1, &buffer[2]);
        if (result != STATUS_OK) return result;
        if ((buffer[2] != responseBuffer[1]) ||
            (buffer[3] != responseBuffer[2])) return STATUS_CRC_WRONG;

        if (responseBuffer[0] & 0x04) {
            cascadeLevel++;
        } else {
            uidComplete = true;
            uid->sak = responseBuffer[0];
        }
    }

    uid->size = (uint8_t)(3 * cascadeLevel + 1);
    return STATUS_OK;
}

MFRC522::StatusCode MFRC522::PICC_HaltA() {
    uint8_t buffer[4];
    buffer[0] = PICC_CMD_HLTA;
    buffer[1] = 0;
    StatusCode result = PCD_CalculateCRC(buffer, 2, &buffer[2]);
    if (result != STATUS_OK) return result;

    result = PCD_TransceiveData(buffer, sizeof(buffer), nullptr, 0);
    if (result == STATUS_TIMEOUT) return STATUS_OK;
    if (result == STATUS_OK)      return STATUS_ERROR;   // PICC should NOT respond
    return result;
}


/* ═══════════════════════════════════════════════════════════════════════════
 *  MIFARE communication
 * ═════════════════════════════════════════════════════════════════════════ */

MFRC522::StatusCode MFRC522::PCD_Authenticate(uint8_t    command,
                                               uint8_t    blockAddr,
                                               MIFARE_Key *key,
                                               Uid        *uid) {
    uint8_t waitIRq = 0x10;   // IdleIRq

    uint8_t sendData[12];
    sendData[0] = command;
    sendData[1] = blockAddr;
    for (uint8_t i = 0; i < MF_KEY_SIZE; i++) {
        sendData[2 + i] = key->keyByte[i];
    }
    for (uint8_t i = 0; i < 4; i++) {
        sendData[8 + i] = uid->uidByte[i + uid->size - 4];
    }

    return PCD_CommunicateWithPICC(PCD_MFAuthent, waitIRq,
                                   sendData, sizeof(sendData));
}

void MFRC522::PCD_StopCrypto1() {
    PCD_ClearRegisterBitMask(Status2Reg, 0x08);   // clear MFCrypto1On
}

MFRC522::StatusCode MFRC522::MIFARE_Read(uint8_t  blockAddr,
                                          uint8_t *buffer,
                                          uint8_t *bufferSize) {
    if (buffer == nullptr || *bufferSize < 18) return STATUS_NO_ROOM;

    buffer[0] = PICC_CMD_MF_READ;
    buffer[1] = blockAddr;
    StatusCode result = PCD_CalculateCRC(buffer, 2, &buffer[2]);
    if (result != STATUS_OK) return result;

    return PCD_TransceiveData(buffer, 4, buffer, bufferSize,
                               nullptr, 0, true);
}

MFRC522::StatusCode MFRC522::MIFARE_Write(uint8_t  blockAddr,
                                           uint8_t *buffer,
                                           uint8_t  bufferSize) {
    if (buffer == nullptr || bufferSize < 16) return STATUS_INVALID;

    uint8_t cmdBuffer[2] = { PICC_CMD_MF_WRITE, blockAddr };
    StatusCode result = PCD_MIFARE_Transceive(cmdBuffer, 2);
    if (result != STATUS_OK) return result;

    return PCD_MIFARE_Transceive(buffer, bufferSize);
}

MFRC522::StatusCode MFRC522::MIFARE_Ultralight_Write(uint8_t  page,
                                                      uint8_t *buffer,
                                                      uint8_t  bufferSize) {
    if (buffer == nullptr || bufferSize < 4) return STATUS_INVALID;

    uint8_t cmdBuffer[6];
    cmdBuffer[0] = PICC_CMD_UL_WRITE;
    cmdBuffer[1] = page;
    memcpy(&cmdBuffer[2], buffer, 4);
    return PCD_MIFARE_Transceive(cmdBuffer, 6);
}

MFRC522::StatusCode MFRC522::MIFARE_Decrement(uint8_t blockAddr, int32_t delta) {
    return MIFARE_TwoStepHelper(PICC_CMD_MF_DECREMENT, blockAddr, delta);
}

MFRC522::StatusCode MFRC522::MIFARE_Increment(uint8_t blockAddr, int32_t delta) {
    return MIFARE_TwoStepHelper(PICC_CMD_MF_INCREMENT, blockAddr, delta);
}

MFRC522::StatusCode MFRC522::MIFARE_Restore(uint8_t blockAddr) {
    return MIFARE_TwoStepHelper(PICC_CMD_MF_RESTORE, blockAddr, 0L);
}

MFRC522::StatusCode MFRC522::MIFARE_TwoStepHelper(uint8_t command,
                                                    uint8_t blockAddr,
                                                    int32_t data) {
    uint8_t cmdBuffer[2] = { command, blockAddr };
    StatusCode result = PCD_MIFARE_Transceive(cmdBuffer, 2);
    if (result != STATUS_OK) return result;

    return PCD_MIFARE_Transceive((uint8_t *)&data, 4, true);
}

MFRC522::StatusCode MFRC522::MIFARE_Transfer(uint8_t blockAddr) {
    uint8_t cmdBuffer[2] = { PICC_CMD_MF_TRANSFER, blockAddr };
    return PCD_MIFARE_Transceive(cmdBuffer, 2);
}

MFRC522::StatusCode MFRC522::MIFARE_GetValue(uint8_t blockAddr,
                                              int32_t *value) {
    uint8_t buffer[18];
    uint8_t size = sizeof(buffer);
    StatusCode status = MIFARE_Read(blockAddr, buffer, &size);
    if (status == STATUS_OK) {
        *value = ((int32_t)buffer[3] << 24) |
                 ((int32_t)buffer[2] << 16) |
                 ((int32_t)buffer[1] <<  8) |
                  (int32_t)buffer[0];
    }
    return status;
}

MFRC522::StatusCode MFRC522::MIFARE_SetValue(uint8_t blockAddr,
                                              int32_t value) {
    uint8_t buffer[18];
    buffer[0]  = buffer[8]  = (uint8_t)(value & 0xFF);
    buffer[1]  = buffer[9]  = (uint8_t)((value >> 8)  & 0xFF);
    buffer[2]  = buffer[10] = (uint8_t)((value >> 16) & 0xFF);
    buffer[3]  = buffer[11] = (uint8_t)((value >> 24) & 0xFF);
    buffer[4]  = ~buffer[0];
    buffer[5]  = ~buffer[1];
    buffer[6]  = ~buffer[2];
    buffer[7]  = ~buffer[3];
    buffer[12] = buffer[14] = blockAddr;
    buffer[13] = buffer[15] = ~blockAddr;
    return MIFARE_Write(blockAddr, buffer, 16);
}

MFRC522::StatusCode MFRC522::PCD_NTAG216_AUTH(uint8_t *passWord,
                                               uint8_t  pACK[]) {
    uint8_t cmdBuffer[18];
    cmdBuffer[0] = 0x1B;
    for (uint8_t i = 0; i < 4; i++) cmdBuffer[i + 1] = passWord[i];

    StatusCode result = PCD_CalculateCRC(cmdBuffer, 5, &cmdBuffer[5]);
    if (result != STATUS_OK) return result;

    uint8_t waitIRq  = 0x30;
    uint8_t validBits = 0;
    uint8_t rxlength  = 5;
    result = PCD_CommunicateWithPICC(PCD_Transceive, waitIRq,
                                     cmdBuffer, 7,
                                     cmdBuffer, &rxlength, &validBits);
    pACK[0] = cmdBuffer[0];
    pACK[1] = cmdBuffer[1];
    return result;
}


/* ═══════════════════════════════════════════════════════════════════════════
 *  Support functions
 * ═════════════════════════════════════════════════════════════════════════ */

/**
 * Wrapper for MIFARE transactions: append CRC, transceive, check MF_ACK.
 */
MFRC522::StatusCode MFRC522::PCD_MIFARE_Transceive(uint8_t *sendData,
                                                    uint8_t  sendLen,
                                                    bool     acceptTimeout) {
    if (sendData == nullptr || sendLen > 16) return STATUS_INVALID;

    uint8_t cmdBuffer[18];
    memcpy(cmdBuffer, sendData, sendLen);
    StatusCode result = PCD_CalculateCRC(cmdBuffer, sendLen,
                                          &cmdBuffer[sendLen]);
    if (result != STATUS_OK) return result;
    sendLen += 2;

    uint8_t waitIRq      = 0x30;
    uint8_t cmdBufferSize = sizeof(cmdBuffer);
    uint8_t validBits     = 0;
    result = PCD_CommunicateWithPICC(PCD_Transceive, waitIRq,
                                     cmdBuffer, sendLen,
                                     cmdBuffer, &cmdBufferSize, &validBits);

    if (acceptTimeout && result == STATUS_TIMEOUT) return STATUS_OK;
    if (result != STATUS_OK) return result;
    if (cmdBufferSize != 1 || validBits != 4) return STATUS_ERROR;
    if (cmdBuffer[0] != MF_ACK) return STATUS_MIFARE_NACK;
    return STATUS_OK;
}

const char *MFRC522::GetStatusCodeName(StatusCode code) {
    switch (code) {
        case STATUS_OK:             return "Success.";
        case STATUS_ERROR:          return "Error in communication.";
        case STATUS_COLLISION:      return "Collision detected.";
        case STATUS_TIMEOUT:        return "Timeout in communication.";
        case STATUS_NO_ROOM:        return "Buffer not big enough.";
        case STATUS_INTERNAL_ERROR: return "Internal error (should not happen).";
        case STATUS_INVALID:        return "Invalid argument.";
        case STATUS_CRC_WRONG:      return "CRC_A mismatch.";
        case STATUS_MIFARE_NACK:    return "MIFARE PICC responded with NAK.";
        default:                    return "Unknown error.";
    }
}

MFRC522::PICC_Type MFRC522::PICC_GetType(uint8_t sak) {
    sak &= 0x7F;
    switch (sak) {
        case 0x04: return PICC_TYPE_NOT_COMPLETE;
        case 0x09: return PICC_TYPE_MIFARE_MINI;
        case 0x08: return PICC_TYPE_MIFARE_1K;
        case 0x18: return PICC_TYPE_MIFARE_4K;
        case 0x00: return PICC_TYPE_MIFARE_UL;
        case 0x10:
        case 0x11: return PICC_TYPE_MIFARE_PLUS;
        case 0x01: return PICC_TYPE_TNP3XXX;
        case 0x20: return PICC_TYPE_ISO_14443_4;
        case 0x40: return PICC_TYPE_ISO_18092;
        default:   return PICC_TYPE_UNKNOWN;
    }
}

const char *MFRC522::PICC_GetTypeName(PICC_Type piccType) {
    switch (piccType) {
        case PICC_TYPE_ISO_14443_4:  return "PICC compliant with ISO/IEC 14443-4";
        case PICC_TYPE_ISO_18092:    return "PICC compliant with ISO/IEC 18092 (NFC)";
        case PICC_TYPE_MIFARE_MINI:  return "MIFARE Mini, 320 bytes";
        case PICC_TYPE_MIFARE_1K:    return "MIFARE 1KB";
        case PICC_TYPE_MIFARE_4K:    return "MIFARE 4KB";
        case PICC_TYPE_MIFARE_UL:    return "MIFARE Ultralight or Ultralight C";
        case PICC_TYPE_MIFARE_PLUS:  return "MIFARE Plus";
        case PICC_TYPE_MIFARE_DESFIRE:return "MIFARE DESFire";
        case PICC_TYPE_TNP3XXX:      return "MIFARE TNP3XXX";
        case PICC_TYPE_NOT_COMPLETE: return "SAK indicates UID is not complete.";
        default:                     return "Unknown type";
    }
}


/* ═══════════════════════════════════════════════════════════════════════════
 *  Debug helpers  (output via printf)
 * ═════════════════════════════════════════════════════════════════════════ */

void MFRC522::PCD_DumpVersionToSerial() {
    uint8_t v = PCD_ReadRegister(VersionReg);
    printf("Firmware Version: 0x%02X", v);
    switch (v) {
        case 0x88: printf(" = (FM17522 clone)\r\n"); break;
        case 0x90: printf(" = v0.0\r\n");            break;
        case 0x91: printf(" = v1.0\r\n");            break;
        case 0x92: printf(" = v2.0\r\n");            break;
        case 0x12: printf(" = counterfeit chip\r\n"); break;
        default:   printf(" = (unknown)\r\n");       break;
    }
    if (v == 0x00 || v == 0xFF) {
        printf("WARNING: Communication failure, is the MFRC522 properly connected?\r\n");
    }
}

void MFRC522::PICC_DumpDetailsToSerial(Uid *uid) {
    printf("Card UID:");
    for (uint8_t i = 0; i < uid->size; i++) {
        printf(" %02X", uid->uidByte[i]);
    }
    printf("\r\nCard SAK: %02X\r\n", uid->sak);
    PICC_Type piccType = PICC_GetType(uid->sak);
    printf("PICC type: %s\r\n", PICC_GetTypeName(piccType));
}

void MFRC522::PICC_DumpToSerial(Uid *uid) {
    MIFARE_Key key;
    PICC_DumpDetailsToSerial(uid);

    PICC_Type piccType = PICC_GetType(uid->sak);
    switch (piccType) {
        case PICC_TYPE_MIFARE_MINI:
        case PICC_TYPE_MIFARE_1K:
        case PICC_TYPE_MIFARE_4K:
            for (uint8_t i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
            PICC_DumpMifareClassicToSerial(uid, piccType, &key);
            break;
        case PICC_TYPE_MIFARE_UL:
            PICC_DumpMifareUltralightToSerial();
            break;
        default:
            printf("Dumping memory contents not implemented for that PICC type.\r\n");
            break;
    }
    printf("\r\n");
    PICC_HaltA();
}

void MFRC522::PICC_DumpMifareClassicToSerial(Uid *uid,
                                              PICC_Type piccType,
                                              MIFARE_Key *key) {
    uint8_t no_of_sectors = 0;
    switch (piccType) {
        case PICC_TYPE_MIFARE_MINI: no_of_sectors =  5; break;
        case PICC_TYPE_MIFARE_1K:   no_of_sectors = 16; break;
        case PICC_TYPE_MIFARE_4K:   no_of_sectors = 40; break;
        default: break;
    }
    if (no_of_sectors) {
        printf("Sector Block   0  1  2  3   4  5  6  7   8  9 10 11  12 13 14 15  AccessBits\r\n");
        for (int8_t i = (int8_t)(no_of_sectors - 1); i >= 0; i--) {
            PICC_DumpMifareClassicSectorToSerial(uid, key, (uint8_t)i);
        }
    }
    PICC_HaltA();
    PCD_StopCrypto1();
}

void MFRC522::PICC_DumpMifareClassicSectorToSerial(Uid *uid,
                                                    MIFARE_Key *key,
                                                    uint8_t sector) {
    StatusCode status;
    uint8_t firstBlock, no_of_blocks;
    bool isSectorTrailer = true;
    bool invertedError   = false;

    uint8_t c1, c2, c3, c1_, c2_, c3_;
    uint8_t g[4], group;
    bool firstInGroup;

    if (sector < 32) {
        no_of_blocks = 4;
        firstBlock   = (uint8_t)(sector * no_of_blocks);
    } else if (sector < 40) {
        no_of_blocks = 16;
        firstBlock   = (uint8_t)(128 + (sector - 32) * no_of_blocks);
    } else {
        return;
    }

    uint8_t buffer[18];
    uint8_t byteCount, blockAddr;

    for (int8_t blockOffset = (int8_t)(no_of_blocks - 1);
         blockOffset >= 0; blockOffset--) {

        blockAddr = (uint8_t)(firstBlock + blockOffset);

        if (isSectorTrailer) {
            printf("%3u   ", sector);
        } else {
            printf("       ");
        }
        printf("%3u  ", blockAddr);

        if (isSectorTrailer) {
            status = PCD_Authenticate(PICC_CMD_MF_AUTH_KEY_A,
                                      firstBlock, key, uid);
            if (status != STATUS_OK) {
                printf("PCD_Authenticate() failed: %s\r\n",
                       GetStatusCodeName(status));
                return;
            }
        }

        byteCount = sizeof(buffer);
        status = MIFARE_Read(blockAddr, buffer, &byteCount);
        if (status != STATUS_OK) {
            printf("MIFARE_Read() failed: %s\r\n", GetStatusCodeName(status));
            continue;
        }

        for (uint8_t i = 0; i < 16; i++) {
            printf(" %02X", buffer[i]);
            if ((i % 4) == 3) printf(" ");
        }

        if (isSectorTrailer) {
            c1  = buffer[7] >> 4;
            c2  = buffer[8] & 0x0F;
            c3  = buffer[8] >> 4;
            c1_ = buffer[6] & 0x0F;
            c2_ = buffer[6] >> 4;
            c3_ = buffer[7] & 0x0F;
            invertedError = (c1 != (~c1_ & 0x0F)) ||
                            (c2 != (~c2_ & 0x0F)) ||
                            (c3 != (~c3_ & 0x0F));
            g[0] = (uint8_t)(((c1 & 1) << 2) | ((c2 & 1) << 1) | (c3 & 1));
            g[1] = (uint8_t)(((c1 & 2) << 1) | (c2 & 2)        | ((c3 & 2) >> 1));
            g[2] = (uint8_t)( (c1 & 4)        | ((c2 & 4) >> 1) | ((c3 & 4) >> 2));
            g[3] = (uint8_t)(((c1 & 8) >> 1)  | ((c2 & 8) >> 2) | ((c3 & 8) >> 3));
            isSectorTrailer = false;
        }

        group        = (no_of_blocks == 4) ? (uint8_t)blockOffset
                                           : (uint8_t)(blockOffset / 5);
        firstInGroup = (no_of_blocks == 4) ||
                       (group == 3) ||
                       (group != (uint8_t)((blockOffset + 1) / 5));

        if (firstInGroup) {
            printf(" [ %u %u %u ]",
                   (g[group] >> 2) & 1,
                   (g[group] >> 1) & 1,
                    g[group]       & 1);
            if (invertedError) printf(" Inverted access bits did not match!");
        }

        if (group != 3 && (g[group] == 1 || g[group] == 6)) {
            int32_t value = ((int32_t)buffer[3] << 24) |
                            ((int32_t)buffer[2] << 16) |
                            ((int32_t)buffer[1] <<  8) |
                             (int32_t)buffer[0];
            printf(" Value=0x%lX Adr=0x%02X", (long)value, buffer[12]);
        }
        printf("\r\n");
    }
}

void MFRC522::PICC_DumpMifareUltralightToSerial() {
    StatusCode status;
    uint8_t buffer[18];
    printf("Page  0  1  2  3\r\n");
    for (uint8_t page = 0; page < 16; page += 4) {
        uint8_t byteCount = sizeof(buffer);
        status = MIFARE_Read(page, buffer, &byteCount);
        if (status != STATUS_OK) {
            printf("MIFARE_Read() failed: %s\r\n", GetStatusCodeName(status));
            break;
        }
        for (uint8_t offset = 0; offset < 4; offset++) {
            uint8_t i = page + offset;
            printf("%3u  ", i);
            for (uint8_t idx = 0; idx < 4; idx++) {
                printf(" %02X", buffer[4 * offset + idx]);
            }
            printf("\r\n");
        }
    }
}


/* ═══════════════════════════════════════════════════════════════════════════
 *  Advanced MIFARE functions
 * ═════════════════════════════════════════════════════════════════════════ */

void MFRC522::MIFARE_SetAccessBits(uint8_t *accessBitBuffer,
                                    uint8_t g0, uint8_t g1,
                                    uint8_t g2, uint8_t g3) {
    uint8_t c1 = (uint8_t)(((g3 & 4) << 1) | ((g2 & 4) << 0) |
                            ((g1 & 4) >> 1) | ((g0 & 4) >> 2));
    uint8_t c2 = (uint8_t)(((g3 & 2) << 2) | ((g2 & 2) << 1) |
                            ((g1 & 2) << 0) | ((g0 & 2) >> 1));
    uint8_t c3 = (uint8_t)(((g3 & 1) << 3) | ((g2 & 1) << 2) |
                            ((g1 & 1) << 1) | ((g0 & 1) << 0));

    accessBitBuffer[0] = (uint8_t)((~c2 & 0x0F) << 4 | (~c1 & 0x0F));
    accessBitBuffer[1] = (uint8_t)(c1 << 4            | (~c3 & 0x0F));
    accessBitBuffer[2] = (uint8_t)(c3 << 4            | c2);
}

bool MFRC522::MIFARE_OpenUidBackdoor(bool logErrors) {
    PICC_HaltA();

    uint8_t cmd      = 0x40;
    uint8_t validBits = 7;
    uint8_t response[32];
    uint8_t received  = sizeof(response);

    StatusCode status = PCD_TransceiveData(&cmd, 1, response, &received,
                                           &validBits, 0, false);
    if (status != STATUS_OK) {
        if (logErrors) {
            printf("Card did not respond to 0x40 after HALT. UID changeable?\r\n");
            printf("Error: %s\r\n", GetStatusCodeName(status));
        }
        return false;
    }
    if (received != 1 || response[0] != 0x0A) {
        if (logErrors) {
            printf("Bad response on backdoor 0x40: 0x%02X (%u valid bits)\r\n",
                   response[0], validBits);
        }
        return false;
    }

    cmd       = 0x43;
    validBits = 8;
    status = PCD_TransceiveData(&cmd, 1, response, &received,
                                &validBits, 0, false);
    if (status != STATUS_OK) {
        if (logErrors) {
            printf("Error at command 0x43: %s\r\n", GetStatusCodeName(status));
        }
        return false;
    }
    if (received != 1 || response[0] != 0x0A) {
        if (logErrors) {
            printf("Bad response on backdoor 0x43: 0x%02X (%u valid bits)\r\n",
                   response[0], validBits);
        }
        return false;
    }
    return true;
}

bool MFRC522::MIFARE_SetUid(uint8_t *newUid, uint8_t uidSize, bool logErrors) {
    if (!newUid || !uidSize || uidSize > 15) {
        if (logErrors) printf("New UID buffer empty or size > 15.\r\n");
        return false;
    }

    MIFARE_Key key;
    for (uint8_t i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

    StatusCode status = PCD_Authenticate(PICC_CMD_MF_AUTH_KEY_A, 1, &key, &uid);
    if (status != STATUS_OK) {
        if (status == STATUS_TIMEOUT) {
            if (!PICC_IsNewCardPresent() || !PICC_ReadCardSerial()) {
                if (logErrors) printf("No card present. Failed to set UID.\r\n");
                return false;
            }
            status = PCD_Authenticate(PICC_CMD_MF_AUTH_KEY_A, 1, &key, &uid);
        }
        if (status != STATUS_OK) {
            if (logErrors) printf("Auth failed: %s\r\n", GetStatusCodeName(status));
            return false;
        }
    }

    uint8_t block0_buffer[18];
    uint8_t byteCount = sizeof(block0_buffer);
    status = MIFARE_Read(0, block0_buffer, &byteCount);
    if (status != STATUS_OK) {
        if (logErrors) {
            printf("MIFARE_Read() failed: %s\r\n", GetStatusCodeName(status));
            printf("Check that KEY A for sector 0 is 0xFFFFFFFFFFFF.\r\n");
        }
        return false;
    }

    uint8_t bcc = 0;
    for (uint8_t i = 0; i < uidSize; i++) {
        block0_buffer[i] = newUid[i];
        bcc ^= newUid[i];
    }
    block0_buffer[uidSize] = bcc;

    PCD_StopCrypto1();

    if (!MIFARE_OpenUidBackdoor(logErrors)) {
        if (logErrors) printf("Activating UID backdoor failed.\r\n");
        return false;
    }

    status = MIFARE_Write(0, block0_buffer, 16);
    if (status != STATUS_OK) {
        if (logErrors) printf("MIFARE_Write() failed: %s\r\n",
                              GetStatusCodeName(status));
        return false;
    }

    uint8_t atqa_answer[2];
    uint8_t atqa_size = 2;
    PICC_WakeupA(atqa_answer, &atqa_size);
    return true;
}

bool MFRC522::MIFARE_UnbrickUidSector(bool logErrors) {
    MIFARE_OpenUidBackdoor(logErrors);

    uint8_t block0_buffer[] = {
        0x01, 0x02, 0x03, 0x04, 0x04,
        0x08, 0x04, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    StatusCode status = MIFARE_Write(0, block0_buffer, 16);
    if (status != STATUS_OK) {
        if (logErrors) printf("MIFARE_Write() failed: %s\r\n",
                              GetStatusCodeName(status));
        return false;
    }
    return true;
}


/* ═══════════════════════════════════════════════════════════════════════════
 *  Convenience wrappers
 * ═════════════════════════════════════════════════════════════════════════ */

bool MFRC522::PICC_IsNewCardPresent() {
    uint8_t bufferATQA[2];
    uint8_t bufferSize = sizeof(bufferATQA);

    PCD_WriteRegister(TxModeReg,   0x00);
    PCD_WriteRegister(RxModeReg,   0x00);
    PCD_WriteRegister(ModWidthReg, 0x26);

    StatusCode result = PICC_RequestA(bufferATQA, &bufferSize);
    return (result == STATUS_OK || result == STATUS_COLLISION);
}

bool MFRC522::PICC_ReadCardSerial() {
    StatusCode result = PICC_Select(&uid);
    return (result == STATUS_OK);
}