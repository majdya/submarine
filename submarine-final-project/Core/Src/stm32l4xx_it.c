/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    stm32l4xx_it.c
 * @brief   Interrupt Service Routines.
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
#include "stm32l4xx_it.h"
#include "app_event.h"
#include "main.h"
#include "uart_rx.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* huart2 is extern-declared further below in this file (after "External
   variables"), but this block needs it earlier - redeclare here too
   (harmless, same extern declaration twice is legal in C). */
extern UART_HandleTypeDef huart2;

/* Captures the exception stack frame + fault status registers on a hard
   fault and dumps them over UART, so the actual fault cause is visible
   without a debugger attached. This never returns. */
typedef struct {
  uint32_t r0, r1, r2, r3, r12, lr, pc, psr;
} FaultStackFrame_t;

void HardFault_Diagnostics(FaultStackFrame_t *frame) {
  uint32_t cfsr = SCB->CFSR;
  uint32_t hfsr = SCB->HFSR;
  uint32_t mmfar = SCB->MMFAR;
  uint32_t bfar = SCB->BFAR;

  char buf[300];
  int len = snprintf(
      buf, sizeof(buf),
      "\r\n*** HARD FAULT ***\r\n"
      "PC=0x%08lX LR=0x%08lX PSR=0x%08lX\r\n"
      "R0=0x%08lX R1=0x%08lX R2=0x%08lX R3=0x%08lX R12=0x%08lX\r\n"
      "CFSR=0x%08lX HFSR=0x%08lX MMFAR=0x%08lX BFAR=0x%08lX\r\n"
      "  MMARVALID=%lu BFARVALID=%lu FORCED=%lu VECTBL=%lu\r\n"
      "  IBUSERR=%lu PRECISERR=%lu IMPRECISERR=%lu UNDEFINSTR=%lu "
      "INVSTATE=%lu INVPC=%lu NOCP=%lu UNALIGNED=%lu DIVBYZERO=%lu\r\n",
      (unsigned long)frame->pc, (unsigned long)frame->lr,
      (unsigned long)frame->psr, (unsigned long)frame->r0,
      (unsigned long)frame->r1, (unsigned long)frame->r2,
      (unsigned long)frame->r3, (unsigned long)frame->r12, (unsigned long)cfsr,
      (unsigned long)hfsr, (unsigned long)mmfar, (unsigned long)bfar,
      (unsigned long)((cfsr >> 7) & 1UL) /* MMARVALID */,
      (unsigned long)((cfsr >> 15) & 1UL) /* BFARVALID */,
      (unsigned long)((hfsr >> 30) & 1UL) /* FORCED */,
      (unsigned long)((hfsr >> 1) & 1UL) /* VECTTBL */,
      (unsigned long)((cfsr >> 8) & 1UL) /* IBUSERR */,
      (unsigned long)((cfsr >> 9) & 1UL) /* PRECISERR */,
      (unsigned long)((cfsr >> 10) & 1UL) /* IMPRECISERR */,
      (unsigned long)((cfsr >> 16) & 1UL) /* UNDEFINSTR */,
      (unsigned long)((cfsr >> 18) & 1UL) /* INVSTATE */,
      (unsigned long)((cfsr >> 19) & 1UL) /* INVPC */,
      (unsigned long)((cfsr >> 20) & 1UL) /* NOCP */,
      (unsigned long)((cfsr >> 24) & 1UL) /* UNALIGNED */,
      (unsigned long)((cfsr >> 25) & 1UL) /* DIVBYZERO */);
  if (len > 0) {
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, (uint16_t)len, HAL_MAX_DELAY);
  }

  __disable_irq();
  while (1) {
  }
}

/* Object Detection and the Silence button are both configured as
   EXTI-falling-edge inputs, but nothing implemented this callback
   before now - the interrupts fired and did nothing. Object
   Detection has no task of its own by design: this ISR is its
   entire implementation, posting straight into the event queue for
   Task_Event to pick up. */
#define BUTTON_DEBOUNCE_MS 200u

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  /* Mechanical switch bounce fires several falling edges per physical
     press - HAL_GetTick() is safe to read from ISR context (it's just the
     SysTick-incremented counter), and unsigned subtraction handles the
     ~49-day wraparound correctly without special-casing it. */
  static uint32_t last_objdetect_tick = 0;
  static uint32_t last_silence_tick = 0;
  /* Button 2 (D3/PB3) stands in for a real distance/IR sensor (see
     hardware.md S7 - no such sensor was available) as a manual
     detected/cleared TOGGLE, since the spec's Object Detection module
     needs both a "detected" and a "cleared" event, not just one signal
     re-fired on every press. */
  static uint8_t object_present = 0;

  uint32_t now = HAL_GetTick();

  if (GPIO_Pin == GPIO_PIN_3) {
    if ((uint32_t)(now - last_objdetect_tick) >= BUTTON_DEBOUNCE_MS) {
      last_objdetect_tick = now;
      object_present = !object_present;
      AppEvent_Post(object_present ? EVENT_OBJECT_DETECTED : EVENT_OBJECT_CLEARED, 0);
    }
  } else if (GPIO_Pin == SILENCE_Pin) {
    if ((uint32_t)(now - last_silence_tick) >= BUTTON_DEBOUNCE_MS) {
      last_silence_tick = now;
      AppEvent_Post(EVENT_SILENCE_PRESSED, 0);
    }
  }
}

/* First-ever UART RX in this project (see uart_rx.c/task_comm_rx.c) -
   HAL calls this once the single byte armed by the last
   HAL_UART_Receive_IT (or UartRx_Start's first call) has arrived. */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART2) {
    UartRx_ByteReceivedFromISR();
  }
}
/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef htim6;

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
 * @brief This function handles Non maskable interrupt.
 */
void NMI_Handler(void) {
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
  while (1) {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
 * @brief This function handles Hard fault interrupt.
 */
void HardFault_Handler(void) {
  /* USER CODE BEGIN HardFault_IRQn 0 */
  __asm volatile("TST LR, #4                \n"
                 "ITE EQ                    \n"
                 "MRSEQ R0, MSP              \n"
                 "MRSNE R0, PSP              \n"
                 "B HardFault_Diagnostics   \n");
  /* USER CODE END HardFault_IRQn 0 */
  while (1) {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
 * @brief This function handles Memory management fault.
 */
void MemManage_Handler(void) {
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1) {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
 * @brief This function handles Prefetch fault, memory access fault.
 */
void BusFault_Handler(void) {
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1) {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
 * @brief This function handles Undefined instruction or illegal state.
 */
void UsageFault_Handler(void) {
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1) {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
 * @brief This function handles Debug monitor.
 */
void DebugMon_Handler(void) {
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32L4xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32l4xx.s).                    */
/******************************************************************************/

/**
 * @brief This function handles EXTI line3 interrupt.
 */
void EXTI3_IRQHandler(void) {
  /* USER CODE BEGIN EXTI3_IRQn 0 */

  /* USER CODE END EXTI3_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3);
  /* USER CODE BEGIN EXTI3_IRQn 1 */

  /* USER CODE END EXTI3_IRQn 1 */
}

/**
 * @brief This function handles USART2 global interrupt.
 */
void USART2_IRQHandler(void) {
  /* USER CODE BEGIN USART2_IRQn 0 */

  /* USER CODE END USART2_IRQn 0 */
  HAL_UART_IRQHandler(&huart2);
  /* USER CODE BEGIN USART2_IRQn 1 */

  /* USER CODE END USART2_IRQn 1 */
}

/**
 * @brief This function handles EXTI line[15:10] interrupts.
 */
void EXTI15_10_IRQHandler(void) {
  /* USER CODE BEGIN EXTI15_10_IRQn 0 */

  /* USER CODE END EXTI15_10_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(SILENCE_Pin);
  /* USER CODE BEGIN EXTI15_10_IRQn 1 */

  /* USER CODE END EXTI15_10_IRQn 1 */
}

/**
 * @brief This function handles TIM6 global interrupt, DAC channel1 and channel2
 * underrun error interrupts.
 */
void TIM6_DAC_IRQHandler(void) {
  /* USER CODE BEGIN TIM6_DAC_IRQn 0 */

  /* USER CODE END TIM6_DAC_IRQn 0 */
  HAL_TIM_IRQHandler(&htim6);
  /* USER CODE BEGIN TIM6_DAC_IRQn 1 */

  /* USER CODE END TIM6_DAC_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
