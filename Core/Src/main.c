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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 定義狀態機列舉：健康、警告、生氣 */
typedef enum {
    HEALTHY,
    WARNING,
    ANGRY
} PetState;
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef hlpuart1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */
/* 定義全域變數，需加上 volatile 避免被編譯器最佳化掉 */
volatile uint32_t sedentary_sec = 0; // 久坐秒數
volatile uint8_t checkin_flag = 0;   // 打卡旗標
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_LPUART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* 3. UART 嘴巴導向：將 printf 強制導向 LPUART1 的 TDR 暫存器 */
void UART_SendString(char* str) {
    while (*str) {
        // 等待 TXE 旗標 (傳輸暫存器為空)，確保上一個字元已送出
        while ((LPUART1->ISR & USART_ISR_TXE) == 0) {}
        LPUART1->TDR = *str++; // 將字元塞入暫存器，並切換到下一個字
    }
}
/* 2. 中斷紀律 (快進快出)：手寫 Timer 3 與 EXTI15_10 中斷服務常式 */
void TIM3_IRQHandler(void) {
    // 檢查 TIM3 UIF (Update Interrupt Flag) 更新中斷旗標
    if (TIM3->SR & TIM_SR_UIF) {
        TIM3->SR &= ~TIM_SR_UIF; // 清除旗標
        sedentary_sec++;         // 久坐秒數加一
    }
}

void EXTI15_10_IRQHandler(void) {
    // 檢查 PC13 (EXTI Line 13) 觸發旗標
    // 註：STM32G4 家族的 EXTI 暫存器為 PR1 (寫入 1 來清除)
    if (EXTI->PR1 & (1 << 13)) {
        EXTI->PR1 = (1 << 13);   // 清除旗標
        sedentary_sec = 0;       // 秒數歸零
        checkin_flag = 1;        // 立下打卡旗標
    }
}

/* 5. Timer 2 呼吸燈頻率控制函式 */
void Set_LED_Speed(PetState state) {
    if (state == HEALTHY) {
        // 健康慢速 (週期長)
        TIM2->PSC = 1700 - 1;
        TIM2->ARR = 1000 - 1;
    } else if (state == WARNING) {
        // 警告快速 (週期中)
        TIM2->PSC = 850 - 1;
        TIM2->ARR = 500 - 1;
    } else {
        // 生氣狂閃 (週期短)
        TIM2->PSC = 170 - 1;
        TIM2->ARR = 100 - 1;
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
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_LPUART1_UART_Init();
  /* USER CODE BEGIN 2 */
    /* USER CODE BEGIN 2 */
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_LPUART1_UART_Init(); // 這行掌管了 PA2, PA3 與 115200 鮑率的設定！
    /* --- 4. Timer 3 碼錶初始化 --- */
    // 供給 Timer 的 APB1 為 85MHz，85MHz / 17000 = 5000Hz。5000Hz / 5000 = 1Hz (精準 1 秒)
    TIM3->PSC = 17000 - 1;
    TIM3->ARR = 5000 - 1;
    TIM3->DIER |= TIM_DIER_UIE; // 開啟 Update Interrupt 允許
    TIM3->CR1 |= TIM_CR1_CEN;   // 啟動 Timer 3
    NVIC_EnableIRQ(TIM3_IRQn);
    NVIC_EnableIRQ(EXTI15_10_IRQn);
    /* --- 5. Timer 2 呼吸燈與防當機初始化 --- */
    Set_LED_Speed(HEALTHY);     // 先呼叫速度設定函式設定好 PSC 與 ARR
    TIM2->EGR |= TIM_EGR_UG;    // 關鍵：強制設定 UG 位元，避免開機死機 25 秒陷阱

    // 啟動 PWM 通道與計時器 (若 CubeMX 已生成可省略，此為保險底層寫法)
    TIM2->CCMR1 &= ~TIM_CCMR1_OC1M_Msk;
    TIM2->CCMR1 |= (0x6 << TIM_CCMR1_OC1M_Pos); // PWM mode 1
    TIM2->CCER |= TIM_CCER_CC1E;                // 開啟通道 1 輸出
    TIM2->CR1 |= TIM_CR1_CEN;                   // 啟動 Timer 2

    /* --- 6. 進入 while 前的初始化與防亂碼 --- */
    for (volatile int i = 0; i < 500000; i++); // 簡單軟體延遲，等待系統與序列埠穩定
    UART_SendString("\r\n=== 久坐健康精靈 開機完成 ===\r\n");

    PetState current_state = HEALTHY;
    int32_t ccr_val = 0;
    int8_t ccr_step = 1;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  // A. 檢查打卡事件
	        if (checkin_flag) {
	            checkin_flag = 0;
	            if (current_state != HEALTHY) {
	                current_state = HEALTHY;
	                Set_LED_Speed(HEALTHY);
	                UART_SendString("感謝安撫！精靈已恢復健康，重新計時。\r\n");
	            }
	        }

	        // B. 狀態機與逾時判斷邏輯
	        if (sedentary_sec > 30 && current_state != ANGRY) {
	            current_state = ANGRY;
	            UART_SendString("【警告】精靈非常生氣！你已經坐超過 30 秒了！\r\n");
	        }
	        else if (sedentary_sec > 15 && sedentary_sec <= 30 && current_state == HEALTHY) {
	            current_state = WARNING;
	            UART_SendString("【提醒】精靈不開心了，久坐超過 15 秒，該起來喝水囉！\r\n");
	        }

	        // C. Timer 2 呼吸燈動態漸變掃描邏輯 (純改變步伐)
	        if (current_state == HEALTHY) {
	            ccr_step = (ccr_step > 0) ? 1 : -1;   // 慢速呼吸 (步伐極小)
	        } else if (current_state == WARNING) {
	            ccr_step = (ccr_step > 0) ? 5 : -5;   // 中速提醒 (步伐中等)
	        } else {
	            ccr_step = (ccr_step > 0) ? 50 : -50; // 生氣狂閃 (步伐極大)
	        }

	        ccr_val += ccr_step;

	        // 反彈邏輯 (上限永遠固定為 1000)
	        if (ccr_val >= 1000) {
	            ccr_val = 1000;
	            ccr_step = -ccr_step;
	        } else if (ccr_val <= 0) {
	            ccr_val = 0;
	            ccr_step = -ccr_step;
	        }

	        TIM2->CCR1 = ccr_val;

	        // 將延遲數字改小，讓動畫更新更順暢
	        for (volatile int i = 0; i < 10000; i++);
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
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
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
}

/**
  * @brief LPUART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_LPUART1_UART_Init(void)
{

  /* USER CODE BEGIN LPUART1_Init 0 */

  /* USER CODE END LPUART1_Init 0 */

  /* USER CODE BEGIN LPUART1_Init 1 */

  /* USER CODE END LPUART1_Init 1 */
  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate = 115200;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits = UART_STOPBITS_1;
  hlpuart1.Init.Parity = UART_PARITY_NONE;
  hlpuart1.Init.Mode = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&hlpuart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&hlpuart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPUART1_Init 2 */

  /* USER CODE END LPUART1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

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
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
