/**
 * MFRC522_STM32.h - MFRC522 RFID library ported to STM32 HAL
 *
 * Original Arduino library by Dr.Leong, Miguel Balboa, Søren Thing Andersen,
 * Tom Clement and many more contributors.
 *
 * STM32 HAL port: replaces all Arduino/AVR-specific calls with STM32 HAL equivalents.
 *
 * Usage:
 *   1. Configure SPI, CS pin and RST pin in STM32CubeMX.
 *   2. Construct: MFRC522 rfid(&hspi1, GPIOA, GPIO_PIN_4, GPIOA, GPIO_PIN_3);
 *   3. Call rfid.PCD_Init() in your main().
 *
 * Notes:
 *   - printf() is used for debug output. Retarget _write() to your UART or SWO.
 *   - DWT cycle counter is used for microsecond delays (auto-initialised in PCD_Init).
 *   - GPIO pins should be configured as GPIO_OUTPUT_PP in CubeMX.
 *     The RST pin direction is briefly switched to INPUT at runtime to detect
 *     power-down state; this is handled internally via HAL_GPIO_Init().
 */

#ifndef MFRC522_STM32_H
#define MFRC522_STM32_H

#include "stm32_hal_includes.h"   // Adjust to your target, e.g. "stm32f4xx_hal.h"
#include <stdint.h>
#include <stddef.h>

/* ── Convenience: adjust this include to match your target family ──────────
   Examples:
     #include "stm32f1xx_hal.h"
     #include "stm32f4xx_hal.h"
     #include "stm32g0xx_hal.h"
   Create a thin wrapper header "stm32_hal_includes.h" that includes the
   correct HAL header, or simply replace the include above.           ──── */

/** SPI clock speed for the MFRC522 (max 10 MHz; we use 4 MHz). */
#ifndef MFRC522_SPITIMEOUT
#define MFRC522_SPITIMEOUT  100U   // HAL SPI timeout in ms
#endif

class MFRC522 {
public:

    /* ── FIFO / pin constants ─────────────────────────────────────────── */
    static constexpr uint8_t FIFO_SIZE  = 64U;
    static constexpr uint8_t UNUSED_PIN = UINT8_MAX;

    /* ── Register map (chapter 9 of the MFRC522 datasheet) ──────────── */
    enum PCD_Register : uint8_t {
        // Page 0: Command and status
        CommandReg       = 0x01 << 1,
        ComIEnReg        = 0x02 << 1,
        DivIEnReg        = 0x03 << 1,
        ComIrqReg        = 0x04 << 1,
        DivIrqReg        = 0x05 << 1,
        ErrorReg         = 0x06 << 1,
        Status1Reg       = 0x07 << 1,
        Status2Reg       = 0x08 << 1,
        FIFODataReg      = 0x09 << 1,
        FIFOLevelReg     = 0x0A << 1,
        WaterLevelReg    = 0x0B << 1,
        ControlReg       = 0x0C << 1,
        BitFramingReg    = 0x0D << 1,
        CollReg          = 0x0E << 1,
        // Page 1: Command
        ModeReg          = 0x11 << 1,
        TxModeReg        = 0x12 << 1,
        RxModeReg        = 0x13 << 1,
        TxControlReg     = 0x14 << 1,
        TxASKReg         = 0x15 << 1,
        TxSelReg         = 0x16 << 1,
        RxSelReg         = 0x17 << 1,
        RxThresholdReg   = 0x18 << 1,
        DemodReg         = 0x19 << 1,
        MfTxReg          = 0x1C << 1,
        MfRxReg          = 0x1D << 1,
        SerialSpeedReg   = 0x1F << 1,
        // Page 2: Configuration
        CRCResultRegH    = 0x21 << 1,
        CRCResultRegL    = 0x22 << 1,
        ModWidthReg      = 0x24 << 1,
        RFCfgReg         = 0x26 << 1,
        GsNReg           = 0x27 << 1,
        CWGsPReg         = 0x28 << 1,
        ModGsPReg        = 0x29 << 1,
        TModeReg         = 0x2A << 1,
        TPrescalerReg    = 0x2B << 1,
        TReloadRegH      = 0x2C << 1,
        TReloadRegL      = 0x2D << 1,
        TCounterValueRegH= 0x2E << 1,
        TCounterValueRegL= 0x2F << 1,
        // Page 3: Test registers
        TestSel1Reg      = 0x31 << 1,
        TestSel2Reg      = 0x32 << 1,
        TestPinEnReg     = 0x33 << 1,
        TestPinValueReg  = 0x34 << 1,
        TestBusReg       = 0x35 << 1,
        AutoTestReg      = 0x36 << 1,
        VersionReg       = 0x37 << 1,
        AnalogTestReg    = 0x38 << 1,
        TestDAC1Reg      = 0x39 << 1,
        TestDAC2Reg      = 0x3A << 1,
        TestADCReg       = 0x3B << 1
    };

    /* ── PCD commands (chapter 10) ───────────────────────────────────── */
    enum PCD_Command : uint8_t {
        PCD_Idle            = 0x00,
        PCD_Mem             = 0x01,
        PCD_GenerateRandomID= 0x02,
        PCD_CalcCRC         = 0x03,
        PCD_Transmit        = 0x04,
        PCD_NoCmdChange     = 0x07,
        PCD_Receive         = 0x08,
        PCD_Transceive      = 0x0C,
        PCD_MFAuthent       = 0x0E,
        PCD_SoftReset       = 0x0F
    };

    /* ── Receiver gain masks ─────────────────────────────────────────── */
    enum PCD_RxGain : uint8_t {
        RxGain_18dB   = 0x00 << 4,
        RxGain_23dB   = 0x01 << 4,
        RxGain_18dB_2 = 0x02 << 4,
        RxGain_23dB_2 = 0x03 << 4,
        RxGain_33dB   = 0x04 << 4,
        RxGain_38dB   = 0x05 << 4,
        RxGain_43dB   = 0x06 << 4,
        RxGain_48dB   = 0x07 << 4,
        RxGain_min    = 0x00 << 4,
        RxGain_avg    = 0x04 << 4,
        RxGain_max    = 0x07 << 4
    };

    /* ── PICC commands ───────────────────────────────────────────────── */
    enum PICC_Command : uint8_t {
        PICC_CMD_REQA          = 0x26,
        PICC_CMD_WUPA          = 0x52,
        PICC_CMD_CT            = 0x88,
        PICC_CMD_SEL_CL1       = 0x93,
        PICC_CMD_SEL_CL2       = 0x95,
        PICC_CMD_SEL_CL3       = 0x97,
        PICC_CMD_HLTA          = 0x50,
        PICC_CMD_RATS          = 0xE0,
        PICC_CMD_MF_AUTH_KEY_A = 0x60,
        PICC_CMD_MF_AUTH_KEY_B = 0x61,
        PICC_CMD_MF_READ       = 0x30,
        PICC_CMD_MF_WRITE      = 0xA0,
        PICC_CMD_MF_DECREMENT  = 0xC0,
        PICC_CMD_MF_INCREMENT  = 0xC1,
        PICC_CMD_MF_RESTORE    = 0xC2,
        PICC_CMD_MF_TRANSFER   = 0xB0,
        PICC_CMD_UL_WRITE      = 0xA2
    };

    /* ── Miscellaneous MIFARE constants ──────────────────────────────── */
    enum MIFARE_Misc {
        MF_ACK      = 0xA,
        MF_KEY_SIZE = 6
    };

    /* ── PICC types ──────────────────────────────────────────────────── */
    enum PICC_Type : uint8_t {
        PICC_TYPE_UNKNOWN,
        PICC_TYPE_ISO_14443_4,
        PICC_TYPE_ISO_18092,
        PICC_TYPE_MIFARE_MINI,
        PICC_TYPE_MIFARE_1K,
        PICC_TYPE_MIFARE_4K,
        PICC_TYPE_MIFARE_UL,
        PICC_TYPE_MIFARE_PLUS,
        PICC_TYPE_MIFARE_DESFIRE,
        PICC_TYPE_TNP3XXX,
        PICC_TYPE_NOT_COMPLETE = 0xFF
    };

    /* ── Status codes ────────────────────────────────────────────────── */
    enum StatusCode : uint8_t {
        STATUS_OK,
        STATUS_ERROR,
        STATUS_COLLISION,
        STATUS_TIMEOUT,
        STATUS_NO_ROOM,
        STATUS_INTERNAL_ERROR,
        STATUS_INVALID,
        STATUS_CRC_WRONG,
        STATUS_MIFARE_NACK = 0xFF
    };

    /* ── Data structures ─────────────────────────────────────────────── */
    typedef struct {
        uint8_t size;          ///< Number of bytes in UID (4, 7 or 10)
        uint8_t uidByte[10];
        uint8_t sak;           ///< Select Acknowledge byte
    } Uid;

    typedef struct {
        uint8_t keyByte[MF_KEY_SIZE];
    } MIFARE_Key;

    /* ── Member variables ────────────────────────────────────────────── */
    Uid uid;

    /* ── Constructors ────────────────────────────────────────────────── */
    /**
     * @param hspi      Pointer to the initialised STM32 HAL SPI handle.
     * @param csPort    GPIO port of the chip-select (NSS) pin.
     * @param csPin     GPIO pin mask of the chip-select pin (e.g. GPIO_PIN_4).
     * @param rstPort   GPIO port of the reset/power-down (NRSTPD) pin.
     *                  Pass nullptr if not connected.
     * @param rstPin    GPIO pin mask of the reset pin.
     *                  Pass 0 / UNUSED_PIN if not connected.
     */
    MFRC522(SPI_HandleTypeDef *hspi,
            GPIO_TypeDef *csPort,  uint16_t csPin,
            GPIO_TypeDef *rstPort, uint16_t rstPin);

    /** Constructor without reset pin (soft reset only). */
    MFRC522(SPI_HandleTypeDef *hspi,
            GPIO_TypeDef *csPort, uint16_t csPin);

    /* ── Basic SPI interface ─────────────────────────────────────────── */
    void    PCD_WriteRegister(PCD_Register reg, uint8_t value);
    void    PCD_WriteRegister(PCD_Register reg, uint8_t count, uint8_t *values);
    uint8_t PCD_ReadRegister(PCD_Register reg);
    void    PCD_ReadRegister(PCD_Register reg, uint8_t count, uint8_t *values,
                             uint8_t rxAlign = 0);
    void    PCD_SetRegisterBitMask(PCD_Register reg, uint8_t mask);
    void    PCD_ClearRegisterBitMask(PCD_Register reg, uint8_t mask);
    StatusCode PCD_CalculateCRC(uint8_t *data, uint8_t length, uint8_t *result);

    /* ── PCD control ─────────────────────────────────────────────────── */
    void PCD_Init();
    void PCD_Reset();
    void PCD_AntennaOn();
    void PCD_AntennaOff();
    uint8_t PCD_GetAntennaGain();
    void    PCD_SetAntennaGain(uint8_t mask);
    bool    PCD_PerformSelfTest();

    /* ── Power control ───────────────────────────────────────────────── */
    void PCD_SoftPowerDown();
    void PCD_SoftPowerUp();

    /* ── PICC communication ──────────────────────────────────────────── */
    StatusCode PCD_TransceiveData(uint8_t *sendData, uint8_t sendLen,
                                  uint8_t *backData, uint8_t *backLen,
                                  uint8_t *validBits = nullptr,
                                  uint8_t rxAlign = 0, bool checkCRC = false);
    StatusCode PCD_CommunicateWithPICC(uint8_t command, uint8_t waitIRq,
                                       uint8_t *sendData, uint8_t sendLen,
                                       uint8_t *backData = nullptr,
                                       uint8_t *backLen  = nullptr,
                                       uint8_t *validBits= nullptr,
                                       uint8_t rxAlign   = 0,
                                       bool    checkCRC  = false);
    StatusCode PICC_RequestA(uint8_t *bufferATQA, uint8_t *bufferSize);
    StatusCode PICC_WakeupA (uint8_t *bufferATQA, uint8_t *bufferSize);
    StatusCode PICC_REQA_or_WUPA(uint8_t command,
                                  uint8_t *bufferATQA, uint8_t *bufferSize);
    virtual StatusCode PICC_Select(Uid *uid, uint8_t validBits = 0);
    StatusCode PICC_HaltA();

    /* ── MIFARE communication ────────────────────────────────────────── */
    StatusCode PCD_Authenticate(uint8_t command, uint8_t blockAddr,
                                MIFARE_Key *key, Uid *uid);
    void       PCD_StopCrypto1();
    StatusCode MIFARE_Read(uint8_t blockAddr, uint8_t *buffer,
                           uint8_t *bufferSize);
    StatusCode MIFARE_Write(uint8_t blockAddr, uint8_t *buffer,
                            uint8_t bufferSize);
    StatusCode MIFARE_Ultralight_Write(uint8_t page, uint8_t *buffer,
                                       uint8_t bufferSize);
    StatusCode MIFARE_Decrement(uint8_t blockAddr, int32_t delta);
    StatusCode MIFARE_Increment(uint8_t blockAddr, int32_t delta);
    StatusCode MIFARE_Restore(uint8_t blockAddr);
    StatusCode MIFARE_Transfer(uint8_t blockAddr);
    StatusCode MIFARE_GetValue(uint8_t blockAddr, int32_t *value);
    StatusCode MIFARE_SetValue(uint8_t blockAddr, int32_t value);
    StatusCode PCD_NTAG216_AUTH(uint8_t *passWord, uint8_t pACK[]);

    /* ── Support / utility ───────────────────────────────────────────── */
    StatusCode  PCD_MIFARE_Transceive(uint8_t *sendData, uint8_t sendLen,
                                      bool acceptTimeout = false);
    static const char *GetStatusCodeName(StatusCode code);
    static PICC_Type   PICC_GetType(uint8_t sak);
    static const char *PICC_GetTypeName(PICC_Type type);

    /* ── Debug helpers (output via printf) ──────────────────────────── */
    void PCD_DumpVersionToSerial();
    void PICC_DumpToSerial(Uid *uid);
    void PICC_DumpDetailsToSerial(Uid *uid);
    void PICC_DumpMifareClassicToSerial(Uid *uid, PICC_Type piccType,
                                        MIFARE_Key *key);
    void PICC_DumpMifareClassicSectorToSerial(Uid *uid, MIFARE_Key *key,
                                              uint8_t sector);
    void PICC_DumpMifareUltralightToSerial();

    /* ── Advanced MIFARE ─────────────────────────────────────────────── */
    void MIFARE_SetAccessBits(uint8_t *accessBitBuffer,
                              uint8_t g0, uint8_t g1,
                              uint8_t g2, uint8_t g3);
    bool MIFARE_OpenUidBackdoor(bool logErrors);
    bool MIFARE_SetUid(uint8_t *newUid, uint8_t uidSize, bool logErrors);
    bool MIFARE_UnbrickUidSector(bool logErrors);

    /* ── Convenience wrappers ────────────────────────────────────────── */
    virtual bool PICC_IsNewCardPresent();
    virtual bool PICC_ReadCardSerial();

protected:
    SPI_HandleTypeDef *_hspi;
    GPIO_TypeDef      *_csPort;
    uint16_t           _csPin;
    GPIO_TypeDef      *_rstPort;
    uint16_t           _rstPin;

    StatusCode MIFARE_TwoStepHelper(uint8_t command, uint8_t blockAddr,
                                    int32_t data);

private:
    /* ── Low-level helpers ───────────────────────────────────────────── */
    static void     InitDWT();
    static void     DelayMicroseconds(uint32_t us);
    void            SetPinAsOutput(GPIO_TypeDef *port, uint16_t pin);
    void            SetPinAsInput (GPIO_TypeDef *port, uint16_t pin);
};

#endif /* MFRC522_STM32_H */