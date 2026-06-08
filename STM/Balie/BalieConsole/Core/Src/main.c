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
uint8_t datacheck = 0;
uint8_t RTRReceived = 0;

static uint8_t  noodActief = 0;
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

void Test(){
	UART_Print("Test");
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


	  if(1){
		  //ZetAan(GPIOB, Dag_Nacht_Pin);
	  }

	  if(1){
		  //ZetAan(GPIOB, TemperatuurOverGrens_Pin);
	  }

	  if(1){
		  //ZetAan(GPIOB, RelaxstoelStatus_Pin);
	  }

	  if(1){
		  //ZetAan(GPIOB, CO2OverGrens_Pin);
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
	if ((RxHeader.StdId == 0x001)) {
        noodActief = !noodActief;
        if (noodActief)
        {
            UART_Print("Noodknop ingedrukt!\r\n\r\n");
            ZetAan(GPIOB, NoodknopLed_Pin);
        }
        else
        {
            UART_Print("Noodstand uit!\r\n\r\n");
            ZetUit(GPIOB, NoodknopLed_Pin);
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

static uint32_t db_Deuren = 0;
static uint32_t db_Nood   = 0;
static uint32_t db_Cyclus = 0;
static uint32_t db_Relax  = 0;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    uint32_t nu = HAL_GetTick();

    // Noodknop altijd verwerken, ongeacht noodActief
    if (GPIO_Pin == Noodstatusknop_Pin && (nu - db_Nood) >= 300)
    {
        db_Nood = nu;
        noodActief = !noodActief;
        if (noodActief)
        {
            UART_Print("Noodknop ingedrukt!\r\n\r\n");
            ZetAan(GPIOB, NoodknopLed_Pin);
			uint64_t StuurNiks = 0x00;
			SendCanMessage(1, StuurNiks, 0x001);
        }
        else
        {
            UART_Print("Noodstand uit!\r\n\r\n");
            ZetUit(GPIOB, NoodknopLed_Pin);
			uint64_t StuurNiks = 0x00;
			SendCanMessage(1, StuurNiks, 0x001);
        }
        return;
    }

    if (noodActief) return;

    if (GPIO_Pin == AlleDeurenOpen_Pin && (nu - db_Deuren) >= 300)
    {
        db_Deuren = nu;
        UART_Print("Alle Deuren gaan open\r\n\r\n");
		uint64_t StuurNiks = 0x00;
		SendCanMessage(1, StuurNiks, 0x120);
    }
    else if (GPIO_Pin == DeurCyclus_Pin && (nu - db_Cyclus) >= 300)
    {
        db_Cyclus = nu;
        UART_Print("Deurcyclus wordt gestart\r\n\r\n");
		uint64_t StuurNiks = 0x00;
		SendCanMessage(1, StuurNiks, 0x130);
    }
    else if (GPIO_Pin == SwitchRelaxstoelStatus_Pin && (nu - db_Relax) >= 300)
    {
        db_Relax = nu;
        UART_Print("Status Relaxstoel veranderd\r\n\r\n");

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
