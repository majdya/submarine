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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_comm.h"
#include "app_event.h"
#include "app_log.h"
#include "app_serial.h"
#include "app_config.h"
#include "app_state.h"
#include "rtc_ds1307.h"
#include "dwt_delay.h"
#include "indicators.h"
#include "peripheral_selftest.h"
#include "log_query.h"
#include "task_comm.h"
#include "task_comm_rx.h"
#include "task_event.h"
#include "task_keepalive.h"
#include "uart_rx.h"
#include "task_log.h"
#include "task_monitor.h"
#include "task_watchdog.h"
#include "task_ir_detect.h"
#include "ir_receiver.h"
#include <stdio.h>
#include <string.h>
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
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;

I2C_HandleTypeDef hi2c3;

IWDG_HandleTypeDef hiwdg;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart2;

/* Definitions for InitTask - one-shot boot sequence, then becomes the
   alive-blink heartbeat for the rest of the run. */
osThreadId_t initTaskHandle;
const osThreadAttr_t initTask_attributes = {
    .name = "InitTask",
    .stack_size = 512 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};

osThreadId_t monitorTaskHandle;
const osThreadAttr_t monitorTask_attributes = {
    .name = "MonitorTask",
    .stack_size = 512 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};

osThreadId_t eventTaskHandle;
const osThreadAttr_t eventTask_attributes = {
    .name = "EventTask",
    .stack_size = 384 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};

osThreadId_t logTaskHandle;
const osThreadAttr_t logTask_attributes = {
    .name = "LogTask",
    .stack_size = 768 * 4, /* FatFs (f_open/f_write/f_mkfs) needs more
                              headroom than plain GPIO/SPI calls */
    .priority = (osPriority_t)osPriorityBelowNormal,
};

osThreadId_t commTaskHandle;
const osThreadAttr_t commTask_attributes = {
    .name = "CommTask",
    .stack_size = 384 * 4,
    .priority = (osPriority_t)osPriorityAboveNormal,
};

osThreadId_t keepAliveTaskHandle;
const osThreadAttr_t keepAliveTask_attributes = {
    .name = "KeepAliveTask",
    .stack_size = 320 * 4,
    .priority = (osPriority_t)osPriorityAboveNormal,
};

osThreadId_t commRxTaskHandle;
const osThreadAttr_t commRxTask_attributes = {
    .name = "CommRxTask",
    .stack_size = 384 * 4,
    .priority = (osPriority_t)osPriorityAboveNormal,
};

osThreadId_t watchdogTaskHandle;
const osThreadAttr_t watchdogTask_attributes = {
    .name = "WatchdogTask",
    .stack_size = 256 * 4,
    .priority = (osPriority_t)osPriorityHigh,
};

osThreadId_t irDetectTaskHandle;
const osThreadAttr_t irDetectTask_attributes = {
    .name = "IrDetectTask",
    .stack_size = 256 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_IWDG_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_ADC2_Init(void);
static void MX_TIM3_Init(void);
static void MX_I2C3_Init(void);
static void MX_SPI1_Init(void);
void StartInitTask(void *argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* MX_IWDG_Init() is commented out again below - re-enabling it caused a
   real boot loop on hardware (see the comment at the call site). Several
   call sites still (correctly) try to refresh it defensively before it
   may exist - this guards every one of them against calling into a
   zero-initialized hiwdg.Instance. */
uint8_t IwdgIsInitialized(void) { return hiwdg.Instance != NULL; }

static void SafeIwdgRefresh(void) {
  if (IwdgIsInitialized()) {
    HAL_IWDG_Refresh(&hiwdg);
  }
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
  /* USER CODE BEGIN 1 */
  // 1. CAPTURE THE RAW CLEAN VALUE IMMEDIATELY AT BOOT

  uint32_t clean_csr = RCC->CSR;

  // 2. CLEAR THE FLAGS SO THE NEXT BOOT IS NOT POLLUTED
  __HAL_RCC_CLEAR_RESET_FLAGS();
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();
  SystemClock_Config();
  PeriphCommonClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  /* REVERTED (again): re-enabling this caused an immediate boot loop on
     real hardware - CSR showed IWDGRSTF every cycle, cut off partway
     through StartInitTask's very first print, well under the expected
     ~4s window. Root cause not yet found (heap/mutex accounting for the
     new app_config module was checked and looks fine on paper; DBGMCU
     not freezing IWDG during a debugger-attached flash/reset is also a
     candidate, since ST-Link can hold the core briefly around reset).
     Disabling again until this is actually diagnosed against real
     hardware behavior instead of guessed at from here. */
  /* MX_IWDG_Init(); */
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_TIM3_Init();
  MX_I2C3_Init();
  MX_SPI1_Init();

  /* USER CODE BEGIN 2 */
  // 3. PRINT USING THE PRE-SAVED CLEAN_CSR VALUE
  {
    char rcmsg[96];
    int rlen = snprintf(rcmsg, sizeof(rcmsg),
                        "TRUE RESET CAUSE: CSR=0x%08lX%s%s%s%s%s%s\r\n",
                        (unsigned long)clean_csr,
                        (clean_csr & RCC_CSR_LPWRRSTF) ? " LOW-POWER" : "",
                        (clean_csr & RCC_CSR_WWDGRSTF) ? " WWDG" : "",
                        (clean_csr & RCC_CSR_IWDGRSTF) ? " IWDG" : "",
                        (clean_csr & RCC_CSR_SFTRSTF) ? " SOFTWARE" : "",
                        (clean_csr & RCC_CSR_BORRSTF) ? " BOR/POR" : "",
                        (clean_csr & RCC_CSR_PINRSTF) ? " PIN/NRST" : "");
    if (rlen > 0) {
      HAL_UART_Transmit(&huart2, (uint8_t *)rcmsg, (uint16_t)rlen,
                        HAL_MAX_DELAY);
    }
  }
  SafeIwdgRefresh();

  /* NOTE: previously this block manually re-initialized PA5 as a plain GPIO
     output for an "alive" blink, which used to collide with SPI1_SCK on
     that same pin. Removed. LED1 (PC5) is already a clean, conflict-free
     GPIO output configured by MX_GPIO_Init() - use it for the alive
     indicator instead (now the alive-blink in StartInitTask). SD card SPI
     stays on SPI1/PA5-6-7 + PB6 CS - the SD/RTC shield's traces are
     hard-wired there and can't be moved, so those pins permanently share
     duty with the sensor shield's RGB LED (Green/Blue) and D12 LED. */

  SafeIwdgRefresh();
  {
    const char *msg = "Submarine LNC boot OK - v0.1 \r\n";
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
  }
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  Serial_Init();
  AppState_Init();
  AppConfig_Init();
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  UartRx_Init();
  UartRx_Start(); /* arms the first single-byte HAL_UART_Receive_IT -
                      safe before osKernelStart(): the semaphore it
                      signals already exists, and FreeRTOS allows giving
                      one before the scheduler is running (the count is
                      just there waiting for Task_CommRx to acquire it
                      once it starts). */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  AppEvent_QueueCreate();
  AppLog_QueueCreate();
  AppComm_QueueCreate();
  LogQuery_QueueCreate();
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of InitTask */
  initTaskHandle = osThreadNew(StartInitTask, NULL, &initTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  monitorTaskHandle = osThreadNew(Task_Monitor, NULL, &monitorTask_attributes);
  eventTaskHandle = osThreadNew(Task_Event, NULL, &eventTask_attributes);
  logTaskHandle = osThreadNew(Task_Log, NULL, &logTask_attributes);
  commTaskHandle = osThreadNew(Task_Comm, NULL, &commTask_attributes);
  keepAliveTaskHandle =
      osThreadNew(Task_KeepAlive, NULL, &keepAliveTask_attributes);
  commRxTaskHandle = osThreadNew(Task_CommRx, NULL, &commRxTask_attributes);
  watchdogTaskHandle =
      osThreadNew(Task_Watchdog, NULL, &watchdogTask_attributes);
  irDetectTaskHandle =
      osThreadNew(Task_IrDetect, NULL, &irDetectTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Close the gap: nothing refreshes IWDG between here and Task_Watchdog's
     first refresh, and osKernelStart() + first context switch is not
     instant. Refresh right before handing off. */
  SafeIwdgRefresh();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* Unreachable once osKernelStart() succeeds - real application code lives
       in StartInitTask() and the other app tasks. This loop only runs if the
       scheduler fails to start, which should never happen; kept empty on
       purpose. */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType =
      RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * @brief Peripherals Common Clock Configuration
 * @retval None
 */
void PeriphCommonClock_Config(void) {
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the peripherals clock
   */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCCLKSOURCE_PLLSAI1;
  PeriphClkInit.PLLSAI1.PLLSAI1Source = RCC_PLLSOURCE_HSI;
  PeriphClkInit.PLLSAI1.PLLSAI1M = 1;
  PeriphClkInit.PLLSAI1.PLLSAI1N = 8;
  PeriphClkInit.PLLSAI1.PLLSAI1P = RCC_PLLP_DIV7;
  PeriphClkInit.PLLSAI1.PLLSAI1Q = RCC_PLLQ_DIV2;
  PeriphClkInit.PLLSAI1.PLLSAI1R = RCC_PLLR_DIV2;
  PeriphClkInit.PLLSAI1.PLLSAI1ClockOut = RCC_PLLSAI1_ADC1CLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * @brief ADC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC1_Init(void) {

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
   */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 2;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK) {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
   */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Regular Channel
   */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_92CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Regular Channel
   */
  sConfig.Channel = ADC_CHANNEL_9;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */
}

/**
 * @brief ADC2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC2_Init(void) {

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */

  /** Common config
   */
  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc2.Init.Resolution = ADC_RESOLUTION_12B;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc2.Init.LowPowerAutoWait = DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc2.Init.DMAContinuousRequests = DISABLE;
  hadc2.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc2.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc2) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Regular Channel
   */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_92CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */
}

/**
 * @brief I2C3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C3_Init(void) {

  /* USER CODE BEGIN I2C3_Init 0 */

  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
  hi2c3.Instance = I2C3;
  hi2c3.Init.Timing = 0x10D19CE4;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Analogue filter
   */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Digital filter
   */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */
}

/**
 * @brief IWDG Initialization Function
 * @param None
 * @retval None
 */
static void MX_IWDG_Init(void) {

  /* USER CODE BEGIN IWDG_Init 0 */

  /* USER CODE END IWDG_Init 0 */

  /* USER CODE BEGIN IWDG_Init 1 */

  /* USER CODE END IWDG_Init 1 */
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
  hiwdg.Init.Window = 0;
  hiwdg.Init.Reload = 2000;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN IWDG_Init 2 */

  /* USER CODE END IWDG_Init 2 */
}

/**
 * @brief SPI1 Initialization Function
 * @param None
 * @retval None
 *
 * Reverted back to SPI1 (PA5/6/7 + PB6 CS): the SD/RTC data-logger
 * shield's SD slot is hard-wired on its PCB to the Arduino SPI pins
 * (D13/D12/D11/D10 = PA5/PA6/PA7/PB6) - it cannot be moved. The other
 * sensor shield's RGB LED sits on D9-D11 of the same header and both
 * shields are stacked together, so D10/D11 (PB6/PA7) are physically
 * shared between the SD card's CS/MOSI and the RGB LED's Green/Blue
 * legs - that overlap is a real hardware limit of this stack, not
 * something firmware can resolve. */
static void MX_SPI1_Init(void) {

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
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */
}

/**
 * @brief TIM3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM3_Init(void) {

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 79;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK) {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);
}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void) {

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
  if (HAL_UART_Init(&huart2) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */
  /* USART2's NVIC interrupt was never actually enabled before - the
     handler existed in stm32l4xx_it.c but nothing unmasked it, so it
     could never fire. Priority 5 matches configMAX_SYSCALL_INTERRUPT_
     PRIORITY (FreeRTOSConfig.h) - the same priority already used for
     EXTI3_IRQn/EXTI15_10_IRQn below, safe for calling FreeRTOS ISR-safe
     APIs (UartRx_ByteReceivedFromISR -> osSemaphoreRelease). */
  HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
  /* USER CODE END USART2_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LED2_Pin | SD_CS_Pin,
                    GPIO_PIN_SET); /* SD_CS_Pin (PB6) idle deselected */

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED1_Pin */
  GPIO_InitStruct.Pin = LED1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED2_Pin SD_CS_Pin (PB6, SPI1 manual CS) */
  GPIO_InitStruct.Pin = LED2_Pin | SD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : SILENCE_Pin */
  GPIO_InitStruct.Pin = SILENCE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SILENCE_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PB3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : IR_RECEIVER_Pin (PB10 / D6) - plain polled
    input, NOT an interrupt: PB10 shares EXTI line 10 with
    SILENCE_Pin (PA10), and only one GPIO port can be routed to a
    given EXTI line number at a time. Task_IrDetect polls this pin
    instead - see task_ir_detect.c. */
  GPIO_InitStruct.Pin = IR_RECEIVER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(IR_RECEIVER_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  /* Priority 0 here (the original CubeMX default) is ABOVE
     configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5, see FreeRTOSConfig.h)
     - an ISR that urgent is not allowed to call any FreeRTOS API
     (osMessageQueuePut, called from HAL_GPIO_EXTI_Callback via
     AppEvent_Post). Doing so corrupts the kernel rather than faulting
     cleanly, which is exactly what 'presses this button, firmware
     hangs, only a hardware reset recovers it' looks like. EXTI3
     (Object Detect) was already correctly set to 5 - match it here. */
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartInitTask */
/**
 * @brief  One-shot boot sequence: confirms the clock, brings up the app's
 *         shared state, and marks the system healthy so Task_Watchdog
 *         starts refreshing IWDG. After that it has no more init work, so
 *         it becomes the alive-blink heartbeat (LED1) for the rest of the
 *         run rather than exiting - a visibly blinking LED is a useful,
 *         zero-cost "still running" indicator distinct from the IWDG.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartInitTask */
void StartInitTask(void *argument) {
  /* USER CODE BEGIN 5 */
  {
    /* Force a fresh recompute of SystemCoreClock straight from the live
       RCC/PLL registers right here, right before printing, instead of
       trusting whatever value the global variable currently holds. This
       tells us definitively whether the hardware clock is really at
       SystemCoreClock's printed value, or whether that global is stale. */
    SystemCoreClockUpdate();
    uint32_t pllcfgr = RCC->PLLCFGR;
    char init_msg[140];
    int ilen = snprintf(
        init_msg, sizeof(init_msg),
        "InitTask: SystemCoreClock=%lu Hz (post-update) PLLCFGR=0x%08lX\r\n",
        (unsigned long)SystemCoreClock, (unsigned long)pllcfgr);
    if (ilen > 0) {
      Serial_Print(init_msg);
    }
  }
  DWT_Init();

  {
    /* Clear the DS1307's clock-halt bit if needed (dead/first-power
       battery), seeding it from this firmware's build timestamp in that
       case only. If the battery-backed clock is already running, its
       time is trusted over the build timestamp and left untouched. */
    uint8_t rtc_was_halted = 0;
    HAL_StatusTypeDef rtc_st = RTC_Init(&rtc_was_halted);
    char rtc_msg[96];
    int rlen;
    if (rtc_st != HAL_OK) {
      rlen = snprintf(rtc_msg, sizeof(rtc_msg),
                       "InitTask: RTC_Init FAILED (I2C error) - logs will use "
                       "FatFs's no-RTC placeholder date\r\n");
    } else if (rtc_was_halted) {
      RTC_DateTime_t dt = RTC_GetBuildDateTime();
      rlen = snprintf(rtc_msg, sizeof(rtc_msg),
                       "InitTask: RTC was halted - set from build time "
                       "%04u-%02u-%02u %02u:%02u:%02u\r\n",
                       dt.year, dt.month, dt.day, dt.hour, dt.min, dt.sec);
    } else {
      rlen = snprintf(rtc_msg, sizeof(rtc_msg),
                       "InitTask: RTC already running (battery-backed time "
                       "trusted)\r\n");
    }
    if (rlen > 0) {
      Serial_Print(rtc_msg);
    }
  }

  /* Everything the other tasks need (mutexes, queues) was already created
     in MX_FREERTOS_Init before any thread started - nothing left to set up
     here. Mark the system healthy so Task_Watchdog begins refreshing. */
  AppState_SetHealthy(1);
  Serial_Print("InitTask: startup complete, system healthy\r\n");

  /* Alive-blink heartbeat - the task's ongoing role from here on. */
  for (;;) {
    LED1_Toggle();
    osDelay(1000);
  }
  /* USER CODE END 5 */
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM6 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
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
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
