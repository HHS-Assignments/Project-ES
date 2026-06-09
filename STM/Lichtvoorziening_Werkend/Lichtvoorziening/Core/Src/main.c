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
#include "ws2812b.h"
#include <string.h>
#include <stdio.h>
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

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim1;
DMA_HandleTypeDef hdma_tim1_ch1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
volatile uint8_t button_pressed = 0;
volatile uint32_t last_sensor_tick = 0;
volatile uint32_t led_on_tick = 0;
volatile uint8_t led_on = 0;
volatile uint32_t canbusTick = 0;

CAN_TxHeaderTypeDef TxHeader;
CAN_RxHeaderTypeDef RxHeader;
uint8_t TxData[8];
uint8_t RxData[8];
uint32_t TxMailbox;
uint8_t datacheck = 0;
uint8_t RTRReceived = 0;

uint8_t huidigeKleur = 2;
static uint8_t noodActief = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_CAN1_Init(void);
/* USER CODE BEGIN PFP */
void SGP30_Init(void);
uint16_t SGP30_ReadCO2(void);
static void chase_tick(int step);
void SHT3x_Read(float *temp);
void SendCanMessage(int dataLength, uint64_t data, uint16_t canID);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void SGP30_Init(void) {
	uint8_t cmd[2] = { 0x20, 0x03 };
	HAL_I2C_Master_Transmit(&hi2c1, 0x58 << 1, cmd, 2, HAL_MAX_DELAY);
}

uint16_t SGP30_ReadCO2(void) {
	uint8_t cmd[2] = { 0x20, 0x08 };
	HAL_I2C_Master_Transmit(&hi2c1, 0x58 << 1, cmd, 2, HAL_MAX_DELAY);

	uint32_t start = HAL_GetTick();
	while (HAL_GetTick() - start < 12)
		;

	uint8_t buf[6];
	HAL_I2C_Master_Receive(&hi2c1, 0x58 << 1, buf, 6, HAL_MAX_DELAY);

	return (uint16_t) ((buf[0] << 8) | buf[1]);
}

static void chase_tick(int step) {
	WS2812B_Clear();
	uint16_t j = (uint16_t) (step % WS2812B_NUM_LEDS);
	uint16_t i = (uint16_t) ((step + 1) % WS2812B_NUM_LEDS);
	WS2812B_SetLED(i, 255, 0, 0);
	WS2812B_SetLED(j, 255, 0, 0);
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
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_TIM1_Init();
  MX_USART2_UART_Init();
  MX_CAN1_Init();
  /* USER CODE BEGIN 2 */
	/* USER CODE BEGIN 2 */
	char startup[] = "STM32 gestart!\r\n";
	HAL_UART_Transmit(&huart2, (uint8_t*) startup, strlen(startup), 1000);
	SGP30_Init();
	WS2812B_Init();

	CAN_FilterTypeDef canfilterconfig;

	/* Common filter settings */
	canfilterconfig.FilterActivation = CAN_FILTER_ENABLE;
	canfilterconfig.FilterFIFOAssignment = CAN_FILTER_FIFO0;
	canfilterconfig.FilterMode = CAN_FILTERMODE_IDMASK;
	canfilterconfig.FilterScale = CAN_FILTERSCALE_32BIT;
	canfilterconfig.SlaveStartFilterBank = 10;

	/* Filter 0 -> 0x200 */
	canfilterconfig.FilterBank = 0;
	canfilterconfig.FilterIdHigh = (0x001 << 5);
	canfilterconfig.FilterIdLow = 0;
	canfilterconfig.FilterMaskIdHigh = (0x7FF << 5);
	canfilterconfig.FilterMaskIdLow = 0;
	HAL_CAN_ConfigFilter(&hcan1, &canfilterconfig);

	/* Filter 1 -> 0x210 */
	canfilterconfig.FilterBank = 1;
	canfilterconfig.FilterIdHigh = (0x100 << 5);
	HAL_CAN_ConfigFilter(&hcan1, &canfilterconfig);

	/* Filter 2 -> 0x001 */
	canfilterconfig.FilterBank = 2;
	canfilterconfig.FilterIdHigh = (0x110 << 5);
	HAL_CAN_ConfigFilter(&hcan1, &canfilterconfig);

	/* Filter 3 -> 0x300 */
	canfilterconfig.FilterBank = 3;
	canfilterconfig.FilterIdHigh = (0x410 << 5);
	HAL_CAN_ConfigFilter(&hcan1, &canfilterconfig);

	/* Filter 4 -> 0x310 */
	canfilterconfig.FilterBank = 4;
	canfilterconfig.FilterIdHigh = (0x420 << 5);
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
	const char msg_beweging[] =
			"Beweging gedetecteerd, verlichting gaat aan!\r\n";
	const char msg_uit[] = "Lamp gaat uit!\r\n";
	int chase_step = 0;
	uint32_t last_chase_tick = 0;
	while (1) {
		if (HAL_GetTick() - last_sensor_tick >= 1000) {
			last_sensor_tick = HAL_GetTick();

			uint16_t eco2 = SGP30_ReadCO2();

			float temp = 0.0f;
			SHT3x_Read(&temp);
			int t_int = (int) temp;
			int t_dec = (int) ((temp - t_int) * 100);

			char uart_msg[96];
			sprintf(uart_msg, "eCO2: %d ppm | Temp: %d.%02d C\r\n", eco2, t_int,
					t_dec);

			HAL_UART_Transmit(&huart2, (uint8_t*) uart_msg, strlen(uart_msg),
					HAL_MAX_DELAY);
		}

		//led strip
		if (HAL_GetTick() - last_chase_tick >= 150) {
			last_chase_tick = HAL_GetTick();
			chase_tick(chase_step);
			chase_step++;
		}

		if (button_pressed) {
			button_pressed = 0;
			HAL_UART_Transmit(&huart2, (uint8_t*) msg_beweging,
					strlen(msg_beweging), HAL_MAX_DELAY);
			switch (huidigeKleur) {
			case 1:
				HAL_GPIO_WritePin(GPIOA, RGB_Rood_Pin, GPIO_PIN_SET);
				break;

			case 2:
				HAL_GPIO_WritePin(GPIOA, RGB_Groen_Pin, GPIO_PIN_SET);
				break;

			case 3:
				HAL_GPIO_WritePin(GPIOA, RGB_Blauw_Pin, GPIO_PIN_SET);
				break;
			}
			led_on_tick = HAL_GetTick();
			led_on = 1;
		}

		if (led_on && (HAL_GetTick() - led_on_tick >= 5000)) {
			HAL_GPIO_WritePin(GPIOA,
					RGB_Groen_Pin | RGB_Blauw_Pin | RGB_Rood_Pin,
					GPIO_PIN_RESET);
			HAL_UART_Transmit(&huart2, (uint8_t*) msg_uit, strlen(msg_uit),
					HAL_MAX_DELAY);
			led_on = 0;
		}

		if (HAL_GetTick() - canbusTick >= 10000) {
			canbusTick = HAL_GetTick();
			uint16_t eco2 = SGP30_ReadCO2();
			float temp = 0.0f;
			SHT3x_Read(&temp);
			int t_int = (int) temp;
			SendCanMessage(2, (uint64_t) eco2, 0x300);
			SendCanMessage(1, (uint64_t) t_int, 0x310);
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
  RCC_OscInitStruct.PLL.PLLN = 16;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
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
  hcan1.Init.TimeSeg1 = CAN_BS1_2TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_1TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
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
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00B07CB4;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

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
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 39;
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
  sConfigOC.Pulse = 0;
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
  huart2.Init.BaudRate = 9600;
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
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);

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
  HAL_GPIO_WritePin(GPIOA, RGB_Rood_Pin|RGB_Blauw_Pin|RGB_Groen_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : Button_Input_Pin */
  GPIO_InitStruct.Pin = Button_Input_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(Button_Input_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : RGB_Rood_Pin RGB_Blauw_Pin RGB_Groen_Pin */
  GPIO_InitStruct.Pin = RGB_Rood_Pin|RGB_Blauw_Pin|RGB_Groen_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LD3_Pin */
  GPIO_InitStruct.Pin = LD3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD3_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
	if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK) {
		Error_Handler();
	}

	char msg[] = "CAN ontvangen!\r\n";
	HAL_UART_Transmit(&huart2, (uint8_t*) msg, sizeof(msg) - 1, HAL_MAX_DELAY);
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
	if ((RxHeader.StdId == 0x100)) {
		if (RxData[0] == 0x01) {
			huidigeKleur = 1;
		} else if (RxData[0] == 0x02) {
			huidigeKleur = 2;
		} else if (RxData[0] == 0x03) {
			huidigeKleur = 3;
		}
	}
}

void SendCanMessage(int dataLength, uint64_t data, uint16_t canID) {
	uint8_t tempData[8] = { data, data >> 8, data >> 16, data >> 24, data >> 32,
			data >> 40, data >> 48, data >> 56 };
	int y = 0;
	for (int i = dataLength - 1; i >= 0; i--) {
		TxData[i] = tempData[y];
		y++;
	}
	TxHeader.DLC = dataLength;
	TxHeader.StdId = canID;

	if (HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox) != HAL_OK) {
		Error_Handler();
	}
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	static uint32_t last_tick = 0;
	if (GPIO_Pin == Button_Input_Pin) {
		uint32_t now = HAL_GetTick();
		if (now - last_tick > 200) {
			button_pressed = 1;
			last_tick = now;
		}
	}
}

void HAL_TIM_PWM_PulseFinishedHalfCpltCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM1)
		WS2812B_DMA_HalfCpltCallback();
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM1)
		WS2812B_DMA_CpltCallback();
}

void SHT3x_Read(float *temp) {
	uint8_t cmd[2] = { 0x24, 0x00 };
	uint8_t data[6];

	HAL_I2C_Master_Transmit(&hi2c1, 0x44 << 1, cmd, 2, HAL_MAX_DELAY);
	HAL_Delay(15);
	HAL_I2C_Master_Receive(&hi2c1, 0x44 << 1, data, 6, HAL_MAX_DELAY);

	uint16_t rawTemp = (data[0] << 8) | data[1];
	*temp = -45.0f + 175.0f * ((float) rawTemp / 65535.0f);
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
