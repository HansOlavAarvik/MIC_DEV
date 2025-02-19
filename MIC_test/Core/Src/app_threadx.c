/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @author  MCD Application Team
  * @brief   ThreadX applicative file
  ******************************************************************************
    * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include <stdio.h>
#include "stm32h5xx_hal_uart.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define AUDIO_BUFFER_SIZE    512
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
extern uint16_t audioBuffer[];
extern volatile uint8_t bufferHalf;
extern I2S_HandleTypeDef hi2s2;
extern DMA_HandleTypeDef handle_GPDMA1_Channel0;
extern UART_HandleTypeDef huart3;

TX_THREAD audio_thread;
TX_SEMAPHORE data_ready_semaphore;
uint8_t audio_thread_stack[2048];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
VOID AudioThread_Entry(ULONG initial_input);
/* USER CODE END PFP */

/**
  * @brief  Application ThreadX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;
  /* USER CODE BEGIN App_ThreadX_MEM_POOL */

  /* USER CODE END App_ThreadX_MEM_POOL */
  /* USER CODE BEGIN App_ThreadX_Init */
  tx_thread_create(&audio_thread, "Audio Thread",
                   AudioThread_Entry, 0,
                   audio_thread_stack, 2048,
                   10, 10, TX_NO_TIME_SLICE, TX_AUTO_START);

  tx_semaphore_create(&data_ready_semaphore, "Audio Data Semaphore", 0);
  /* USER CODE END App_ThreadX_Init */

  return ret;
}

  /**
  * @brief  Function that implements the kernel's initialization.
  * @param  None
  * @retval None
  */
void MX_ThreadX_Init(void)
{
  /* USER CODE BEGIN Before_Kernel_Start */

  /* USER CODE END Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN Kernel_Start_Error */

  /* USER CODE END Kernel_Start_Error */
}

/* USER CODE BEGIN 1 */
VOID AudioThread_Entry(ULONG initial_input)
{
    /* Wait for I2S and DMA initialization to complete */
    tx_thread_sleep(10);

    /* Start DMA transfer */
    printf("Starting audio capture...\n");
    if (HAL_I2S_Receive_DMA(&hi2s2, (uint16_t*)audioBuffer, AUDIO_BUFFER_SIZE/2) != HAL_OK)
    {
        printf("Error starting DMA\n");
        return;
    }

    /* Processing loop */
    uint32_t timeout_count = 0;

    while (1)
    {
        /* Wait for new data with timeout */
        UINT status = tx_semaphore_get(&data_ready_semaphore, 200);

        if (status == TX_SUCCESS)
        {
            /* Calculate start of buffer half */
            uint16_t* samples = bufferHalf ?
                               &audioBuffer[AUDIO_BUFFER_SIZE/2] :
                               &audioBuffer[0];

            /* Print audio level (simple peak detection) */
            int16_t peak = 0;
            for (int i = 0; i < 64; i++)
            {
                int16_t value = (int16_t)(samples[i] >> 1);
                if (value < 0) value = -value;  // absolute value
                if (value > peak) peak = value;
            }

            // In your processing loop
            for (int i = 0; i < 10; i++) {
                char buffer[20];
                snprintf(buffer, sizeof(buffer), "%d\r\n", samples[i]);
                HAL_UART_Transmit(&huart3, (uint8_t*)buffer, strlen(buffer), 10);
            }
        }
        else
        {
            /* Timeout handling */
            timeout_count++;
            printf("Timeout #%lu waiting for audio data\n", timeout_count);

            if (timeout_count >= 3) {
                /* Restart DMA after multiple timeouts */
                HAL_I2S_DMAStop(&hi2s2);
                HAL_I2S_Receive_DMA(&hi2s2, (uint16_t*)audioBuffer, AUDIO_BUFFER_SIZE/2);
                timeout_count = 0;
            }
        }
    }
}
/* USER CODE END 1 */
