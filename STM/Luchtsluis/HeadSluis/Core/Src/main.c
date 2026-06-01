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
#include "MFRC522_STM32.h"
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

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi3;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uint8_t uid[4];
extern uint8_t atqa[];

typedef enum {
	STATE_NORMAL, STATE_EMERGENCY
} system_state_t;

volatile system_state_t state = STATE_NORMAL;

const uint8_t BinnendeurDicht = 25;
const uint8_t BinnendeurOpen = 70;
const uint8_t BuitendeurDicht = 30;
const uint8_t BuitendeurOpen = 71;

volatile uint32_t lastNoodButtonTime = 0;

CAN_TxHeaderTypeDef TxHeader;
CAN_RxHeaderTypeDef RxHeader;
uint8_t TxData[8];
uint8_t RxData[8];
uint32_t TxMailbox;
uint8_t datacheck = 0;
uint8_t RTRReceived = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI3_Init(void);
static void MX_SPI1_Init(void);
static void MX_CAN1_Init(void);
/* USER CODE BEGIN PFP */
MFRC522_t rfID = { &hspi1, CS_GPIO_Port, CS_Pin, RESET_GPIO_Port, RESET_Pin };
void SendCanMessage(int dataLength, uint64_t data, uint16_t canID);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int _write(int fd, unsigned char *buf, int len);

void max7219_send(uint8_t reg, uint8_t data) {
	HAL_GPIO_WritePin(MAX_CS_GPIO_Port, MAX_CS_Pin, GPIO_PIN_RESET);

	uint8_t bytes[2] = { reg, data };
	HAL_SPI_Transmit(&hspi3, bytes, 2, HAL_MAX_DELAY);

	HAL_GPIO_WritePin(MAX_CS_GPIO_Port, MAX_CS_Pin, GPIO_PIN_SET);
}

void max7219_init(void) {
	max7219_send(0x0F, 0x00); // Display test uit
	max7219_send(0x0C, 0x01); // Shutdown uit (display aan)
	max7219_send(0x0B, 0x07); // Scan limit: 8 rijen
	max7219_send(0x09, 0x00); // Geen decode mode
	max7219_send(0x0A, 0x08); // Brightness
}
uint8_t leeg_2d[8][8] = { { 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0,
				0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0,
				0, 0, 0 } };

uint8_t uitroepteken_2d[8][8] = { { 0, 0, 0, 1, 1, 0, 0, 0 }, { 0, 0, 0, 1, 1,
		0, 0, 0 }, { 0, 0, 0, 1, 1, 0, 0, 0 }, { 0, 0, 0, 1, 1, 0, 0, 0 }, { 0,
		0, 0, 1, 1, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 1, 1, 0,
		0, 0 }, { 0, 0, 0, 1, 1, 0, 0, 0 } };

uint8_t duimpje_2d[8][8] = { { 0, 0, 0, 0, 1, 0, 0, 0 }, { 0, 0, 0, 1, 1, 0, 0,
		0 }, { 0, 0, 0, 1, 1, 0, 0, 0 }, { 1, 1, 1, 1, 1, 1, 1, 0 }, { 1, 1, 1,
		1, 1, 1, 1, 0 }, { 1, 1, 1, 1, 1, 1, 1, 0 }, { 1, 1, 1, 1, 1, 1, 0, 0 },
		{ 0, 0, 0, 1, 1, 0, 0, 0 } };
uint8_t vinkje_2d[8][8] = { { 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 1 }, { 0, 0, 0, 0, 0, 0, 1, 0 }, { 0, 0, 0, 0, 0,
				1, 0, 0 }, { 1, 0, 0, 0, 1, 0, 0, 0 },
		{ 0, 1, 0, 1, 0, 0, 0, 0 }, { 0, 0, 1, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0,
				0, 0, 0 } };

uint8_t kruisje_2d[8][8] = { { 0, 0, 0, 0, 0, 0, 0, 0 }, { 1, 0, 0, 0, 0, 0, 0,
		1 }, { 0, 1, 0, 0, 0, 0, 1, 0 }, { 0, 0, 1, 0, 0, 1, 0, 0 }, { 0, 0, 0,
		1, 1, 0, 0, 0 }, { 0, 0, 1, 0, 0, 1, 0, 0 }, { 0, 1, 0, 0, 0, 0, 1, 0 },
		{ 1, 0, 0, 0, 0, 0, 0, 1 } };

uint8_t row_to_byte(uint8_t row[8]) {
	uint8_t value = 0;
	for (int i = 0; i < 8; i++) {
		value <<= 1;
		value |= (row[i] & 0x01);
	}
	return value;
}

void max7219_show_2d(uint8_t arr[8][8]) {
	for (uint8_t row = 0; row < 8; row++) {
		uint8_t byte = row_to_byte(arr[row]);
		max7219_send(row + 1, byte);
	}
}

void KaiDeurAutomatiseringBuitenBinnen() {
	char RFIDmsg[] = "Deur 1 gaat open\r\n";
	HAL_UART_Transmit(&huart2, (uint8_t*) RFIDmsg, sizeof(RFIDmsg) - 1, HAL_MAX_DELAY);

	for (int pos = BuitendeurDicht; pos <= BuitendeurOpen; pos++) {
		TIM1->CCR1 = pos;
		HAL_Delay(20);
	}

	HAL_Delay(3000);
	char RFIDmsg1[] = "Deur 1 gaat dicht\r\n";
	HAL_UART_Transmit(&huart2, (uint8_t*) RFIDmsg1, sizeof(RFIDmsg1) - 1, HAL_MAX_DELAY);

	for (int pos = BuitendeurOpen; pos >= BuitendeurDicht; pos--) {
		TIM1->CCR1 = pos;
		HAL_Delay(20);
	}

	char RFIDmsg2[] = "Deur 2 gaat open\r\n";
	HAL_UART_Transmit(&huart2, (uint8_t*) RFIDmsg2, sizeof(RFIDmsg2) - 1, HAL_MAX_DELAY);

	for (int pos = BinnendeurDicht; pos <= BinnendeurOpen; pos++) {
		TIM1->CCR2 = pos;
		HAL_Delay(20);
	}

	HAL_Delay(3000);
	char RFIDmsg3[] = "Deur 2 gaat dicht\r\n";
	HAL_UART_Transmit(&huart2, (uint8_t*) RFIDmsg3, sizeof(RFIDmsg3) - 1, HAL_MAX_DELAY);

	for (int pos = BinnendeurOpen; pos >= BinnendeurDicht; pos--) {
		TIM1->CCR2 = pos;
		HAL_Delay(20);
	}

}

void KaiDeurAutomatiseringBinnenBuiten() {
	char RFIDmsg2[] = "Deur 2 gaat open\r\n";
	HAL_UART_Transmit(&huart2, (uint8_t*) RFIDmsg2, sizeof(RFIDmsg2) - 1, HAL_MAX_DELAY);

	for (int pos = BinnendeurDicht; pos <= BinnendeurOpen; pos++) {
		TIM1->CCR2 = pos;
		HAL_Delay(20);
	}

	HAL_Delay(3000);
	char RFIDmsg3[] = "Deur 2 gaat dicht\r\n";
	HAL_UART_Transmit(&huart2, (uint8_t*) RFIDmsg3, sizeof(RFIDmsg3) - 1, HAL_MAX_DELAY);

	for (int pos = BinnendeurOpen; pos >= BinnendeurDicht; pos--) {
		TIM1->CCR2 = pos;
		HAL_Delay(20);
	}

	char RFIDmsg[] = "Deur 1 gaat open\r\n";
	HAL_UART_Transmit(&huart2, (uint8_t*) RFIDmsg, sizeof(RFIDmsg) - 1, HAL_MAX_DELAY);

	for (int pos = BuitendeurDicht; pos <= BuitendeurOpen; pos++) {
		TIM1->CCR1 = pos;
		HAL_Delay(20);
	}

	HAL_Delay(3000);
	char RFIDmsg1[] = "Deur 1 gaat dicht\r\n";
	HAL_UART_Transmit(&huart2, (uint8_t*) RFIDmsg1, sizeof(RFIDmsg1) - 1, HAL_MAX_DELAY);

	for (int pos = BuitendeurOpen; pos >= BuitendeurDicht; pos--) {
		TIM1->CCR1 = pos;
		HAL_Delay(20);
	}
}

void ResetDeuren() {
	TIM1->CCR2 = BinnendeurDicht;
	TIM1->CCR1 = BuitendeurDicht;
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
  MX_TIM1_Init();
  MX_USART2_UART_Init();
  MX_SPI3_Init();
  MX_SPI1_Init();
  MX_CAN1_Init();
  /* USER CODE BEGIN 2 */
  max7219_init();
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  MFRC522_Init(&rfID);
  ResetDeuren();

  CAN_FilterTypeDef canfilterconfig;

  canfilterconfig.FilterActivation = CAN_FILTER_ENABLE;
  canfilterconfig.FilterBank = 0;  // which filter bank to use from the assigned ones
  canfilterconfig.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  canfilterconfig.FilterIdHigh = 0x113<<5;
  canfilterconfig.FilterIdLow = 0;
  canfilterconfig.FilterMaskIdHigh = 0x7FC<<5;
  canfilterconfig.FilterMaskIdLow = 0x0000;
  canfilterconfig.FilterMode = CAN_FILTERMODE_IDMASK;
  canfilterconfig.FilterScale = CAN_FILTERSCALE_32BIT;
  canfilterconfig.SlaveStartFilterBank = 10;  // how many filters to assign to the CAN1 (master can)

  HAL_CAN_ConfigFilter(&hcan1, &canfilterconfig);

  if (HAL_CAN_Start(&hcan1) != HAL_OK) {
  	  Error_Handler();
  }

  TxHeader.IDE = CAN_ID_STD;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.DLC = 1;

  if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
  {
  	  Error_Handler();
  }
  HAL_Delay(100);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		switch (state) {
		case STATE_NORMAL:
			ResetDeuren();
			max7219_show_2d(leeg_2d);
			if (MFRC522_RequestA(&rfID, atqa) == STATUS_OK) {
				USER_LOG("CARD ID:%02X %02X %02X %02X", uid[0], uid[1], uid[2], uid[3]);
				if (MFRC522_ReadUid(&rfID, uid) == STATUS_OK) {
//					USER_LOG("CARD ID:%02X %02X %02X %02X", uid[0], uid[1], uid[2], uid[3]);
					if ((uid[0] == 0x91) && (uid[1] == 0xE0) && (uid[2] == 0x76) && (uid[3] == 0x1D)) {
						max7219_show_2d(vinkje_2d);
						KaiDeurAutomatiseringBuitenBinnen();
						max7219_show_2d(leeg_2d);
					} else {
						max7219_show_2d(kruisje_2d);
						char RFIDmsg_fout[] = "Toegang geweigerd\r\n";
						HAL_UART_Transmit(&huart2, (uint8_t*) RFIDmsg_fout,
								sizeof(RFIDmsg_fout) - 1, HAL_MAX_DELAY);
						HAL_Delay(2000);
						max7219_show_2d(leeg_2d);
					}
				}
			}

			if (HAL_GPIO_ReadPin(Motion_Input_GPIO_Port, Motion_Input_Pin) == GPIO_PIN_RESET) {
				KaiDeurAutomatiseringBinnenBuiten();
			}
			break;
		case STATE_EMERGENCY:
			max7219_show_2d(uitroepteken_2d);
			TIM1->CCR1 = BuitendeurOpen;
			break;
		}
		uint64_t StuurSensor = 0xAABB;
		SendCanMessage(2, StuurSensor, 0x112);
		HAL_Delay(100);
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
  hcan1.Init.Prescaler = 10;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_13TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = ENABLE;
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
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI3_Init(void)
{

  /* USER CODE BEGIN SPI3_Init 0 */

  /* USER CODE END SPI3_Init 0 */

  /* USER CODE BEGIN SPI3_Init 1 */

  /* USER CODE END SPI3_Init 1 */
  /* SPI3 parameter configuration*/
  hspi3.Instance = SPI3;
  hspi3.Init.Mode = SPI_MODE_MASTER;
  hspi3.Init.Direction = SPI_DIRECTION_2LINES;
  hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi3.Init.NSS = SPI_NSS_SOFT;
  hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi3.Init.CRCPolynomial = 7;
  hspi3.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi3.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI3_Init 2 */

  /* USER CODE END SPI3_Init 2 */

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
  htim1.Init.Prescaler = 1597;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 1000;
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
  sConfigOC.Pulse = 1500;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
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
  HAL_GPIO_WritePin(GPIOA, MAX_CS_Pin|CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(RESET_GPIO_Port, RESET_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : Motion_Input_Pin */
  GPIO_InitStruct.Pin = Motion_Input_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(Motion_Input_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : MAX_CS_Pin CS_Pin */
  GPIO_InitStruct.Pin = MAX_CS_Pin|CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : RESET_Pin */
  GPIO_InitStruct.Pin = RESET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(RESET_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : NoodButton_Input_Pin */
  GPIO_InitStruct.Pin = NoodButton_Input_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(NoodButton_Input_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
int _write(int fd, unsigned char *buf, int len) {
	if (fd == 1 || fd == 2) {                     // stdout or stderr ?
		HAL_UART_Transmit(&huart2, buf, len, 999);  // Print to the UART
	}
	return len;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == NoodButton_Input_Pin) {
        uint32_t now = HAL_GetTick();
        // Ignore interrupts within 200 ms
        if ((now - lastNoodButtonTime) < 200) {
            return;
        }
        lastNoodButtonTime = now;
        if (HAL_GPIO_ReadPin(NoodButton_Input_GPIO_Port, NoodButton_Input_Pin) == GPIO_PIN_RESET) {
            // noodknop logica
            if (state == STATE_NORMAL) {
                state = STATE_EMERGENCY;
                max7219_show_2d(uitroepteken_2d);
                char nood[] = "Noodknop ingedrukt, alle deuren gaan dicht\r\n";
                HAL_UART_Transmit(&huart2, (uint8_t*)nood, sizeof(nood) - 1, HAL_MAX_DELAY);
            }
            else if (state == STATE_EMERGENCY) {
                state = STATE_NORMAL;
                max7219_show_2d(leeg_2d);
            }
        }
    }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK) {
    Error_Handler();
  }
  if ((RxHeader.StdId == 0x113)) {
	  if (RxData[0] == 0x01) {
		  datacheck = 1;
	  } else if (RxData[0] == 0x02) {
		  datacheck =0;
	  }
  }

  if (RxHeader.RTR == CAN_RTR_REMOTE && RxHeader.StdId == 0x112) {
	  RTRReceived = 1;
  }
}

void SendCanMessage(int dataLength, uint64_t data, uint16_t canID) {
	uint8_t tempData[8] = {
			data,
			data >> 8,
			data >> 16,
			data >> 24,
			data >> 32,
			data >> 40,
			data >> 48,
			data >> 56
	};
	int y = 0;
	for (int i = dataLength - 1; i >= 0 ; i--) {
		TxData[i] = tempData[y];
		y++;
	}
	TxHeader.DLC = dataLength;
	TxHeader.StdId = canID;

	if (HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox) != HAL_OK) {
		Error_Handler ();
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
	while (1) {
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
