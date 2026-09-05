/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Shared peripheral handles, defined in main.c, used by the app/ modules. */
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern I2C_HandleTypeDef hi2c3;
extern IWDG_HandleTypeDef hiwdg;
extern SPI_HandleTypeDef hspi3;
extern TIM_HandleTypeDef htim3;
extern UART_HandleTypeDef huart2;

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ADC_BATTERY_Pin GPIO_PIN_0
#define ADC_BATTERY_GPIO_Port GPIOA
#define ADC_LIGHT_Pin GPIO_PIN_1
#define ADC_LIGHT_GPIO_Port GPIOA
#define VCP_to_PC_Pin GPIO_PIN_2
#define VCP_to_PC_GPIO_Port GPIOA
#define VCP_from_PC_Pin GPIO_PIN_3
#define VCP_from_PC_GPIO_Port GPIOA
#define ADC_TEMP_Pin GPIO_PIN_4
#define ADC_TEMP_GPIO_Port GPIOA
#define SD_CS_Pin GPIO_PIN_2
#define SD_CS_GPIO_Port GPIOD
#define LED1_Pin GPIO_PIN_5
#define LED1_GPIO_Port GPIOC
#define Humidity_ADC_Pin GPIO_PIN_0
#define Humidity_ADC_GPIO_Port GPIOB
#define LED2_Pin GPIO_PIN_2
#define LED2_GPIO_Port GPIOB
#define SILENCE_Pin GPIO_PIN_10
#define SILENCE_GPIO_Port GPIOA
#define SILENCE_EXTI_IRQn EXTI15_10_IRQn

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
