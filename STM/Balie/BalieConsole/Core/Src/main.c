/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan1;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
CAN_TxHeaderTypeDef TxHeader;
CAN_RxHeaderTypeDef RxHeader;

uint8_t TxData[8];
uint8_t RxData[8];

uint32_t TxMailbox;
uint8_t rxByte; //usart recieving

uint8_t datacheck = 0;
uint8_t RTRReceived = 0;

volatile uint8_t noodActief = 0;
volatile uint8_t relaxStoelStatus = 0;
volatile uint8_t co2High = 1;
volatile uint8_t tempHigh = 1;
volatile uint8_t timeOfDay = 0;

/* debounce timers */
volatile uint32_t db_Deuren = 0;
volatile uint32_t db_Nood   = 0;
volatile uint32_t db_Cyclus = 0;
volatile uint32_t db_Relax  = 0;

// print usart output variables:
#define MAX_MSGS 5
#define MSG_LEN  32

char meldingen[MAX_MSGS][MSG_LEN];
uint8_t msgIndex = 0;
uint32_t lastRefresh = 0;
volatile uint8_t screenUpdateFlag = 1;
volatile uint16_t co2 = 0;
volatile uint8_t temp = 0;
volatile uint8_t menuState = 0;
// 0 = main menu
// 1 = color menu
// 2 = brightness menu
// 3 = combo menu
char uartBuf[25];
uint8_t uartIdx = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM1_Init(void);
static void MX_CAN1_Init(void);
/* USER CODE BEGIN PFP */
void SendCanMessage(int dataLength, uint64_t data, uint16_t canID);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


void Safe_Delay(int ms)
{
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < ms) {
        if (noodActief) return;
    }
}

void ZetAan(GPIO_TypeDef* GPIO, uint16_t pin) {
    HAL_GPIO_WritePin(GPIO, pin, GPIO_PIN_SET);
}

void ZetUit(GPIO_TypeDef* GPIO, uint16_t pin) {
    HAL_GPIO_WritePin(GPIO, pin, GPIO_PIN_RESET);
}

void UART_Print(const char* str)
{
    while (*str)
    {
        HAL_UART_Transmit(&huart2, (uint8_t*)str, 1, HAL_MAX_DELAY);
        str++;
    }
}

void AddMelding(const char *msg)
{
    // shift older messages down
    for (int i = MAX_MSGS - 1; i > 0; i--)
    {
        strncpy(meldingen[i], meldingen[i - 1], MSG_LEN);
    }

    // insert newest at top
    strncpy(meldingen[0], msg, MSG_LEN - 1);
    meldingen[0][MSG_LEN - 1] = '\0';
}

void SendColor(uint8_t color)
{
    TxHeader.StdId = 0x100;
    TxHeader.DLC = 1;

    TxData[0] = color;

    HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox);
}

void SendBrightness(uint8_t percent)
{
    if (percent > 100) percent = 100;

    uint8_t value = (percent * 255) / 100;

    TxHeader.StdId = 0x110;
    TxHeader.DLC = 1;

    TxData[0] = value;

    HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox);
}

void HandleCommand(char *cmd)
{
    // MAIN MENU
    if (menuState == 0)
    {
        if (cmd[0] == '1')
        {
            menuState = 1;
            UART_Print("\r\nKleur kiezen (0-11):\r\n");
            UART_Print("0: Blauw\r\n");
            UART_Print("1: Groen\r\n");
            UART_Print("2: Aqua\r\n");
            UART_Print("3: Rood\r\n");
            UART_Print("4: Paars\r\n");
            UART_Print("5: Amber\r\n");
            UART_Print("6: Licht Blauw\r\n");
            UART_Print("7: Mint groen\r\n");
            UART_Print("8: Roze\r\n");
            UART_Print("9: Licht paars\r\n");
            UART_Print("10: Licht Geel\r\n");
            UART_Print("11: Wit\r\n");
            UART_Print("Selected color: ");
        }
        else if (cmd[0] == '2')
        {
            menuState = 2;
            UART_Print("\r\nHelderheid kiezen (0-100):\r\n");
        }
        else if (cmd[0] == '3')
        {
            menuState = 3;
            UART_Print("\r\nCombo: kleur + helderheid (bv: 5 80)\r\n");
            UART_Print("0: Blauw\r\n");
            UART_Print("1: Groen\r\n");
            UART_Print("2: Aqua\r\n");
            UART_Print("3: Rood\r\n");
            UART_Print("4: Paars\r\n");
            UART_Print("5: Amber\r\n");
            UART_Print("6: Licht Blauw\r\n");
            UART_Print("7: Mint groen\r\n");
            UART_Print("8: Roze\r\n");
            UART_Print("9: Licht paars\r\n");
            UART_Print("10: Licht Geel\r\n");
            UART_Print("11: Wit\r\n");
        }
        else if (cmd[0] == '4')
        {
            menuState = 4;
            uartIdx = 0; // reset buffer

            UART_Print("\r\nTyp tekst voor lichtkrant (max 24 chars):\r\n");
            UART_Print("> ");
            return;
        }
        else if (cmd[0] == '5')
        {
            menuState = 5;
            uartIdx = 0;

            UART_Print("\r\nNOODTEKST MODE (max 24 chars)\r\n");
            UART_Print("Typ noodbericht:\r\n> ");
            return;
        }
        else if (cmd[0] == '6')
        {
            menuState = 6;
            uartIdx = 0;

            UART_Print("\r\nDag/Nacht tijd instellen\r\n");
            UART_Print("Formaat: 0800 1700\r\n");
            UART_Print("Typ: <dagtijd> <nachtijd>\r\n> ");
            return;
        }
        return;
    }

    // COLOR MENU
    if (menuState == 1)
    {
        int color = atoi(cmd);

        if (color >= 0 && color <= 11)
        {
            SendColor((uint8_t)color);

            screenUpdateFlag = 1;
        }

        menuState = 0;
        return;
    }

    // BRIGHTNESS MENU
    if (menuState == 2)
    {
        int percent = atoi(cmd);

        if (percent < 0) percent = 0;
        if (percent > 100) percent = 100;

        SendBrightness((uint8_t)percent);
        screenUpdateFlag = 1;

        menuState = 0;
        return;
    }

    // COMBO MENU
    if (menuState == 3)
    {
        int color, brightness;

        if (sscanf(cmd, "%d %d", &color, &brightness) == 2)
        {
            if (color >= 0 && color <= 11)
                SendColor((uint8_t)color);

            if (brightness < 0) brightness = 0;
            if (brightness > 100) brightness = 100;

            SendBrightness((uint8_t)brightness);

            UART_Print("Combo sent\r\n");
            screenUpdateFlag = 1;
        }
        else
        {
            UART_Print("Format: <color> <brightness>\r\n");
        }

        menuState = 0;
        return;
    }

    if (menuState == 4)
    {
        int len = strlen(cmd);

        if (len == 0)
        {
            menuState = 0;
            return;
        }

        if (len > 24)
        {
            UART_Print("\r\nTe lang! Max 24 karakters.\r\n");
            len = 24;
        }

        UART_Print("\r\nVersturen...\r\n");

        uint8_t frame1[8] = {0};
        uint8_t frame2[8] = {0};
        uint8_t frame3[8] = {0};

        // ----------------------------
        // Split string into 3 CAN frames
        // ----------------------------
        for (int i = 0; i < 8; i++)
        {
            frame1[i] = (i < len) ? cmd[i] : 0x00;
            frame2[i] = ((i + 8) < len) ? cmd[i + 8] : 0x00;
            frame3[i] = ((i + 16) < len) ? cmd[i + 16] : 0x00;
        }

        // ----------------------------
        // PACK FRAME 1
        // ----------------------------
        uint64_t pack1 = 0;
        uint64_t pack2 = 0;
        uint64_t pack3 = 0;

        for (int i = 0; i < 8; i++)
        {
            pack1 |= ((uint64_t)frame1[i]) << (8 * (7 - i));
            pack2 |= ((uint64_t)frame2[i]) << (8 * (7 - i));
            pack3 |= ((uint64_t)frame3[i]) << (8 * (7 - i));
        }

        // ----------------------------
        // SEND FRAMES
        // ----------------------------
        SendCanMessage(8, pack1, 0x180);
        SendCanMessage(8, pack2, 0x190);
        SendCanMessage(8, pack3, 0x191);

        UART_Print("Klaar met versturen\r\n");

        menuState = 0;
        screenUpdateFlag = 1;
        return;
    }
    if (menuState == 5)
    {
        int len = strlen(cmd);

        if (len == 0)
        {
            menuState = 0;
            return;
        }

        if (len > 24)
        {
            UART_Print("\r\nTe lang! Max 24 karakters.\r\n");
            len = 24;
        }

        UART_Print("\r\nNOODTEKST WORDT VERSTUURD...\r\n");

        uint8_t frame1[8] = {0};
        uint8_t frame2[8] = {0};
        uint8_t frame3[8] = {0};

        // ----------------------------
        // Split into 3 CAN frames
        // ----------------------------
        for (int i = 0; i < 8; i++)
        {
            frame1[i] = (i < len) ? cmd[i] : 0x00;
            frame2[i] = ((i + 8) < len) ? cmd[i + 8] : 0x00;
            frame3[i] = ((i + 16) < len) ? cmd[i + 16] : 0x00;
        }

        // ----------------------------
        // PACK INTO uint64_t SAFELY
        // ----------------------------
        uint64_t pack1 = 0;
        uint64_t pack2 = 0;
        uint64_t pack3 = 0;

        for (int i = 0; i < 8; i++)
        {
            pack1 |= ((uint64_t)frame1[i]) << (8 * (7 - i));
            pack2 |= ((uint64_t)frame2[i]) << (8 * (7 - i));
            pack3 |= ((uint64_t)frame3[i]) << (8 * (7 - i));
        }

        // ----------------------------
        // SEND ALL FRAMES
        // ----------------------------
        SendCanMessage(8, pack1, 0x150);
        SendCanMessage(8, pack2, 0x160);
        SendCanMessage(8, pack3, 0x170);

        UART_Print("NOODTEKST verzonden\r\n");

        menuState = 0;
        screenUpdateFlag = 1;
        return;
    }
    if (menuState == 6)
    {
        int day, night;

        if (strlen(cmd) == 0)
        {
            menuState = 0;
            return;
        }

        // Expect: 0800 1700
        if (sscanf(cmd, "%d %d", &day, &night) != 2)
        {
            UART_Print("\r\n? Fout formaat!\r\nGebruik: 0800 1700\r\n");
            menuState = 0;
            return;
        }

        int dayHH = day / 100;
        int dayMM = day % 100;
        int nightHH = night / 100;
        int nightMM = night % 100;

        if (dayHH > 23 || nightHH > 23 || dayMM > 59 || nightMM > 59)
        {
            UART_Print("\r\n? Ongeldige tijd!\r\n");
            menuState = 0;
            return;
        }

        UART_Print("\r\nTijd versturen...\r\n");

        uint8_t frame[8] = {0};

        // same layout you wanted:
        // 0x00 08 00 00 01 07 00 00

        frame[0] = 0x00;
        frame[1] = (uint8_t)dayHH;
        frame[2] = (uint8_t)dayMM;
        frame[3] = 0x00;

        frame[4] = 0x00;
        frame[5] = (uint8_t)nightHH;
        frame[6] = (uint8_t)nightMM;
        frame[7] = 0x00;

        // ----------------------------
        // PACK USING SAME METHOD AS MENU 5
        // ----------------------------
        uint64_t pack = 0;

        for (int i = 0; i < 8; i++)
        {
            pack |= ((uint64_t)frame[i]) << (8 * (7 - i));
        }

        SendCanMessage(8, pack, 0x192);

        UART_Print("OK: tijd verzonden (0x192)\r\n");

        menuState = 0;
        screenUpdateFlag = 1;
        return;
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  MX_CAN1_Init();
  /* USER CODE BEGIN 2 */
  HAL_UART_Receive_IT(&huart2, &rxByte, 1);

  CAN_FilterTypeDef canfilterconfig;

  /* Common filter settings */
  canfilterconfig.FilterActivation = CAN_FILTER_ENABLE;
  canfilterconfig.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  canfilterconfig.FilterMode = CAN_FILTERMODE_IDMASK;
  canfilterconfig.FilterScale = CAN_FILTERSCALE_32BIT;
  canfilterconfig.SlaveStartFilterBank = 10;

  /* Filter 0 -> 0x200 */
  canfilterconfig.FilterBank = 0;
  canfilterconfig.FilterIdHigh = (0x200 << 5);
  canfilterconfig.FilterIdLow = 0;
  canfilterconfig.FilterMaskIdHigh = (0x7FF << 5);
  canfilterconfig.FilterMaskIdLow = 0;
  HAL_CAN_ConfigFilter(&hcan1, &canfilterconfig);

  /* Filter 1 -> 0x210 */
  canfilterconfig.FilterBank = 1;
  canfilterconfig.FilterIdHigh = (0x210 << 5);
  HAL_CAN_ConfigFilter(&hcan1, &canfilterconfig);

  /* Filter 2 -> 0x001 */
  canfilterconfig.FilterBank = 2;
  canfilterconfig.FilterIdHigh = (0x001 << 5);
  HAL_CAN_ConfigFilter(&hcan1, &canfilterconfig);

  /* Filter 3 -> 0x300 */
  canfilterconfig.FilterBank = 3;
  canfilterconfig.FilterIdHigh = (0x300 << 5);
  HAL_CAN_ConfigFilter(&hcan1, &canfilterconfig);

  /* Filter 4 -> 0x310 */
  canfilterconfig.FilterBank = 4;
  canfilterconfig.FilterIdHigh = (0x310 << 5);
  HAL_CAN_ConfigFilter(&hcan1, &canfilterconfig);

  /* Filter 5 -> 0x400 */
  canfilterconfig.FilterBank = 5;
  canfilterconfig.FilterIdHigh = (0x400 << 5);
  HAL_CAN_ConfigFilter(&hcan1, &canfilterconfig);

  /* Filter 6 -> 0x410 */
  canfilterconfig.FilterBank = 6;
  canfilterconfig.FilterIdHigh = (0x410 << 5);
  HAL_CAN_ConfigFilter(&hcan1, &canfilterconfig);

  /* Filter 7 -> 0x420 */
  canfilterconfig.FilterBank = 7;
  canfilterconfig.FilterIdHigh = (0x420 << 5);
  HAL_CAN_ConfigFilter(&hcan1, &canfilterconfig);

  /* Filter 8 -> 0x430 */
  canfilterconfig.FilterBank = 8;
  canfilterconfig.FilterIdHigh = (0x430 << 5);
  HAL_CAN_ConfigFilter(&hcan1, &canfilterconfig);

	if (HAL_CAN_Start(&hcan1) != HAL_OK) {
		Error_Handler();
	}

	TxHeader.IDE = CAN_ID_STD;
	TxHeader.RTR = CAN_RTR_DATA;
	TxHeader.DLC = 1;

	if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING)
			!= HAL_OK) {
		Error_Handler();
	}
	HAL_Delay(100);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1)
	{
	    uint32_t now = HAL_GetTick();

	    if (((now - lastRefresh) >= 10000 || screenUpdateFlag) && menuState == 0)
	    {
	        lastRefresh = now;
	        screenUpdateFlag = 0;

	        char buf[128];

	        /* Push old content away */
	        for (int i = 0; i < 40; i++)
	        {
	            UART_Print("\r\n");
	        }
	        UART_Print("\r\n==================================================\r\n");

	        UART_Print("           SYSTEEM STATUS OVERVIEW\r\n");
	        UART_Print("==================================================\r\n\r\n");

	        /* ================= KLIMAAT ================= */
	        snprintf(buf, sizeof(buf),
	            "[Klimaat]\r\n"
	            "  CO2  : %d ppm\r\n"
	            "  Temp : %d C\r\n"
	            "  LDR  : %d\r\n\r\n",
	            co2, temp, 0
	        );
	        UART_Print(buf);

	        /* ================= STATUS ================= */
	        snprintf(buf, sizeof(buf),
	            "[Status]\r\n"
	            "  Dag/Nacht : %s\r\n"
	            "  CO2       : %s\r\n"
	            "  Temp      : %s\r\n\r\n",
	            timeOfDay ? "Nacht" : "Dag",
	            co2High ? "WARN" : "OK",
	            tempHigh ? "WARN" : "OK"
	        );
	        UART_Print(buf);

	        /* ================= MELDINGEN ================= */
	        UART_Print("[Meldingen]\r\n");

	        int hasMsg = 0;

	        for (int i = 0; i < MAX_MSGS; i++)
	        {
	            if (meldingen[i][0] != '\0')
	            {
	                hasMsg = 1;

	                snprintf(buf, sizeof(buf),
	                    "  %d) %s",
	                    i + 1,
	                    meldingen[i]
	                );

	                UART_Print(buf);
	            }
	        }

	        if (!hasMsg)
	        {
	            UART_Print("  (geen meldingen)\r\n");
	        }

	        UART_Print("\r\n==================================================\r\n");
	        UART_Print("MENU\r\n");
	        UART_Print("==================================================\r\n");

	        UART_Print("1 - LED kleur instellen\r\n");
	        UART_Print("2 - LED helderheid instellen\r\n");
	        UART_Print("3 - LED kleur + helderheid\r\n");
	        UART_Print("4 - Lichtkrant tekst\r\n");
	        UART_Print("5 - NOODTEKST (prioriteit)\r\n");
	        UART_Print("6 - Stuur dag/nacht tijd\r\n");
	        UART_Print("\r\n==================================================\r\n");
	    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable MSI Auto calibration
  */
  HAL_RCCEx_EnableMSIPLLMode();
}

/**
  * @brief CAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 16;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_3TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_6TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = ENABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 31;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 999;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 499;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, CO2OverGrens_Pin|NoodknopLed_Pin|LD3_Pin|RelaxstoelStatus_Pin
                          |TemperatuurOverGrens_Pin|Dag_Nacht_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : Noodstatusknop_Pin DeurCyclus_Pin SwitchRelaxstoelStatus_Pin AlleDeurenOpen_Pin */
  GPIO_InitStruct.Pin = Noodstatusknop_Pin|DeurCyclus_Pin|SwitchRelaxstoelStatus_Pin|AlleDeurenOpen_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : CO2OverGrens_Pin NoodknopLed_Pin LD3_Pin RelaxstoelStatus_Pin
                           TemperatuurOverGrens_Pin Dag_Nacht_Pin */
  GPIO_InitStruct.Pin = CO2OverGrens_Pin|NoodknopLed_Pin|LD3_Pin|RelaxstoelStatus_Pin
                          |TemperatuurOverGrens_Pin|Dag_Nacht_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        HAL_UART_Transmit(&huart2, &rxByte, 1, HAL_MAX_DELAY);

        if (rxByte == '\r' || rxByte == '\n')
        {
            uartBuf[uartIdx] = '\0';
            uartIdx = 0;

            HandleCommand(uartBuf);
        }
        else
        {
            if (uartIdx < sizeof(uartBuf) - 1)
                uartBuf[uartIdx++] = rxByte;
        }

        HAL_UART_Receive_IT(&huart2, &rxByte, 1);
    }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
	screenUpdateFlag = 1;

	if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK) {
	}

//	char msg[] = "CAN ontvangen!\r\n";
//	HAL_UART_Transmit(&huart2, (uint8_t*) msg, sizeof(msg) - 1, HAL_MAX_DELAY);
//  if ((RxHeader.StdId == 0x113)) {
//	  if (RxData[0] == 0x01) {
//		  datacheck = 1;
//	  } else if (RxData[0] == 0x02) {
//		  datacheck =0;
//	  }
//  }

//  if (RxHeader.RTR == CAN_RTR_REMOTE && RxHeader.StdId == 0x112) {
//	  RTRReceived = 1;
//  }
	if ((RxHeader.StdId == 0x001)) {
		if (RxData[0] == 0x01) {
			noodActief = 1;
			AddMelding("Noodknop aan!\r\n\r\n");
			ZetAan(GPIOB, NoodknopLed_Pin);
		}
		if (RxData[0] == 0x00) {
			noodActief = 0;
			AddMelding("Noodstand uit!\r\n\r\n");
			ZetUit(GPIOB, NoodknopLed_Pin);
		}
	}
	if (RxHeader.StdId == 0x300) {
	    uint16_t value = ((uint16_t)RxData[0] << 8) | RxData[1];
	    if (value >= 600) {
	        if (!co2High) {
	        	AddMelding("CO2 te hoog\r\n\r\n");
	            ZetAan(GPIOB, CO2OverGrens_Pin);
	            co2High = 1;
	        }
	    } else {
	        if (co2High) {
	        	AddMelding("CO2 normaal\r\n\r\n");
	            ZetUit(GPIOB, CO2OverGrens_Pin);
	            co2High = 0;
	        }
	    }
	    co2 = value;
	}
	if (RxHeader.StdId == 0x310) {
	    if (RxData[0] >= 25) {
	    	if (!tempHigh) {
	    		AddMelding("Temperatuur te hoog\r\n\r\n");
	            ZetAan(GPIOB, TemperatuurOverGrens_Pin);
	            tempHigh = 1;
	    	}
	    } else {
	    	if (tempHigh) {
	    		AddMelding("Temperatuur normaal\r\n\r\n");
	            ZetUit(GPIOB, TemperatuurOverGrens_Pin);
	            tempHigh = 0;
	    	}
	    }
	    temp = RxData[0];
	}
	if (RxHeader.StdId == 0x430) {
	    if (RxData[0] == 0x01) {
	    	AddMelding("RelaxStoel aan\r\n\r\n");
	    	ZetAan(GPIOB, RelaxstoelStatus_Pin);
	    	relaxStoelStatus = 1;
	    }
	    else if (RxData[0] == 0x00) {
	    	AddMelding("RelaxStoel uit\r\n\r\n");
	    	ZetUit(GPIOB, RelaxstoelStatus_Pin);
	    	relaxStoelStatus = 0;
	    }
	}
	if (RxHeader.StdId == 0x410) {
		AddMelding("Het is dag\r\n\r\n");
		timeOfDay = 0;
	}
	if (RxHeader.StdId == 0x420) {
		AddMelding("Het is nacht\r\n\r\n");
		timeOfDay = 1;
	}
}

void SendCanMessage(int dataLength, uint64_t data, uint16_t canID)
{
    uint8_t TxData[8] = {0};

    // Extract bytes in BIG-ENDIAN order from uint64_t
    for (int i = 0; i < dataLength; i++) {
        TxData[i] = (data >> (8 * (dataLength - 1 - i))) & 0xFF;
    }

    TxHeader.DLC = dataLength;
    TxHeader.StdId = canID;

    HAL_StatusTypeDef status =
        HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox);

    if (status != HAL_OK) {
        UART_Print("CAN TX failed\r\n");
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    uint32_t nu = HAL_GetTick();
    // Noodknop altijd verwerken, ongeacht noodActief
    if (GPIO_Pin == Noodstatusknop_Pin && (nu - db_Nood) >= 300) {
        db_Nood = nu;
        screenUpdateFlag = 1;
        noodActief = !noodActief;
        if (noodActief)
        {
        	AddMelding("Noodknop aan!\r\n\r\n");
            ZetAan(GPIOB, NoodknopLed_Pin);
			uint64_t NoodknopAan = 0x01;
			SendCanMessage(1, NoodknopAan, 0x001);
        }
        else
        {
        	AddMelding("Noodstand uit!\r\n\r\n");
            ZetUit(GPIOB, NoodknopLed_Pin);
			uint64_t NoodknopUit = 0x00;
			SendCanMessage(1, NoodknopUit, 0x001);
        }
        return;
    }

    if (noodActief) return;

    if (GPIO_Pin == AlleDeurenOpen_Pin && (nu - db_Deuren) >= 300)
    {
    	screenUpdateFlag = 1;
        db_Deuren = nu;
        AddMelding("Alle Deuren gaan open\r\n\r\n");
		uint64_t StuurNiks = 0x00;
		SendCanMessage(1, StuurNiks, 0x120);
    }
    else if (GPIO_Pin == DeurCyclus_Pin && (nu - db_Cyclus) >= 300)
    {
    	screenUpdateFlag = 1;
        db_Cyclus = nu;
        AddMelding("Deurcyclus wordt gestart\r\n\r\n");
		uint64_t StuurNiks = 0x00;
		SendCanMessage(1, StuurNiks, 0x130);
    }
    else if (GPIO_Pin == SwitchRelaxstoelStatus_Pin && (nu - db_Relax) >= 300)
    {
    	screenUpdateFlag = 1;
        db_Relax = nu;
		if (relaxStoelStatus == 0) {
			relaxStoelStatus = 1;
			AddMelding("Status Relaxstoel aan\r\n\r\n");
			uint64_t AanBit = 0x01;
			SendCanMessage(1, AanBit, 0x140);
			ZetAan(GPIOB, RelaxstoelStatus_Pin);
			relaxStoelStatus = 1;
		} else if (relaxStoelStatus == 1) {
			relaxStoelStatus = 0;
			AddMelding("Status Relaxstoel uit\r\n\r\n");
			uint64_t UitBit = 0x00;
			SendCanMessage(1, UitBit, 0x140);
            ZetUit(GPIOB, RelaxstoelStatus_Pin);
            relaxStoelStatus = 0;
		}
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
