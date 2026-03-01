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
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "defines.h"
#include "stdarg.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "stm32f4xx_hal.h" // Example for F4

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define dotTime       60
#define interTime     dotTime
#define dashTime      3*dotTime
#define spaceTime     7*dotTime

void dot();
void dash();
uint8_t morse(const char *format, ... );
uint8_t cdcprintf(const char *format, ... );

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
extern USBD_HandleTypeDef hUsbDeviceFS;
extern uint8_t UserTxBufferFS;
extern uint8_t CDC_IsConnected;


uint8_t buffx[129]  = {0x00};                         //TX buffer
uint32_t sofCnt = 0;
uint32_t oldSofCnt = 0;

// __attribute__((section(".rodata"))) const char* morseCode[] = {
const char* morseCode[] = {
    "-----",            // 0
    ".----",            // 1
    "..---",            // 2
    "...--",            // 3
    "....-",            // 4
    ".....",            // 5
    "-....",            // 6
    "--...",            // 7
    "---..",            // 8
    "----.",            // 9
    ".-",               // A
    "-...",             // B
    "-.-.",             // C
    "-..",              // D
    ".",                // E
    "..-.",             // F
    "--.",              // G
    "....",             // H
    "..",               // I
    ".---",             // J
    "-.-",              // K
    ".-..",             // L
    "--",               // M
    "-.",               // N
    "---",              // O
    ".--.",             // P
    "--.-",             // Q
    ".-.",              // R
    "...",              // S
    "-",                // T
    "..-",              // U
    "...-",             // V
    ".--",              // W
    "-..-",             // X
    "-.--",             // Y
    "--..",             // Z
};   //morse code from A to Z

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

//
void volatile_memset(volatile void *s, int c, size_t n) {
    volatile unsigned char *p = (volatile unsigned char *)s;
    while (n-- > 0) {
        *p++ = (unsigned char)c;
    }
}

void dot()
{
    HAL_GPIO_WritePin(LED_USER_GPIO_Port, LED_USER_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BUZZ_GPIO_Port, BUZZ_Pin, GPIO_PIN_RESET);

    HAL_Delay(dotTime);

    HAL_GPIO_WritePin(LED_USER_GPIO_Port, LED_USER_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(BUZZ_GPIO_Port, BUZZ_Pin, GPIO_PIN_SET);

    HAL_Delay(interTime);
}

void dash()
{
    HAL_GPIO_WritePin(LED_USER_GPIO_Port, LED_USER_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BUZZ_GPIO_Port, BUZZ_Pin, GPIO_PIN_RESET);

    HAL_Delay(dashTime);

    HAL_GPIO_WritePin(BUZZ_GPIO_Port, BUZZ_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_USER_GPIO_Port, LED_USER_Pin, GPIO_PIN_SET);

    HAL_Delay(interTime);
}

//
uint8_t cdcprintf(const char *format, ... )
{
    // USE_HAL_PCD_REGISTER_CALLBACKS
    if (hpcd_USB_OTG_FS.USB_Address == 0) {
        return  1;      //fixme smooker
    }

    if ( !CDC_IsConnected ) {
        return 2;
    }

    uint8_t result = USBD_FAIL;

    va_list ap;

    volatile_memset(buffx, 0, sizeof(buffx));

    int vsprintfResult;

    va_start(ap, format);
    vsprintfResult = vsprintf((char*)&buffx[0], format, ap);
    if ( vsprintfResult < 0 ) {
        BKPT;
    }
    va_end(ap);
    uint8_t len = strlen((const char*)buffx);

    while ( (result != USBD_OK) & (CDC_IsConnected) ) {
        // here smooker. fixme
        result = CDC_Transmit_FS(buffx, (uint16_t)len);
    }

    return result; //
}


uint8_t morse(const char *format, ... )
{
    uint8_t result = 0;

    unsigned char lettInMorse[8];

    uint8_t cnt2 = 0;

    uint8_t symbol;
    uint8_t tmpsymbol;

    while ( (symbol = format[cnt2]) > 0)
    {
        tmpsymbol = symbol;
        //
        if ( symbol == 0x00 ) {
            break;
        }

        // check for alhpa
        if ( ( symbol > 90 ) | ( symbol < 65 ) ) {
          // check for digits
          if ( ( symbol > 57 ) | ( symbol < 48 ) ) {
            if ( symbol != 32 ) {
              BKPT;
              break;
            }
          }
        }

        if ( ( symbol <= 57 ) & ( symbol >= 48 ) ) {
          symbol -= 48;
        }
        if ( ( symbol <= 90 ) & ( symbol >= 65 ) ) {
          symbol -= 55;
        }
        if (symbol == 32) {
            HAL_Delay(spaceTime);
            cdcprintf("%c", tmpsymbol);
            cnt2++;
            continue;
        }
        cdcprintf("%c", tmpsymbol);

        uint8_t index = 0;        //indexLett in letter

        while (1) {
            lettInMorse[index] = morseCode[symbol][index];
            if (lettInMorse[index] == 0) {        // end of string
                break;
            }
            if (lettInMorse[index] == '.') {      //dot
                dot();
            }
            if (lettInMorse[index] == '-') {      //dash
                dash();
            }
            index++;
        }
        HAL_Delay(spaceTime);
        cnt2++;
    }
    cdcprintf("\t%d\t%d\r\n ", sofCnt, sofCnt-oldSofCnt);
    oldSofCnt = sofCnt;
    return result; //
}
void My_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
    UNUSED(hpcd);
    BKPT;
}

void My_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
    UNUSED(hpcd);
    // BKPT;
}

void My_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
    UNUSED(hpcd);
    // BKPT;
}

void delay() {
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
}

void My_PCD_SOF(PCD_HandleTypeDef *hpcd)
{
    UNUSED(hpcd);
    // HAL_GPIO_TogglePin(LED_USER_GPIO_Port, LED_USER_Pin);
    HAL_GPIO_WritePin(LED_USER_GPIO_Port, LED_USER_Pin, GPIO_PIN_RESET);
    delay();
    HAL_GPIO_WritePin(LED_USER_GPIO_Port, LED_USER_Pin, GPIO_PIN_SET);
    // BKPT;
    sofCnt++;
}

void debugStruc()
{
    USB_CfgTypeDef *usb = &hpcd_USB_OTG_FS.Init;
    cdcprintf("#########################################\r\n");
    cdcprintf("hpcd_USB_OTG_FS.Init.dev_endpoints:%d\r\n", usb->dev_endpoints);
    cdcprintf("hpcd_USB_OTG_FS.Init.Host_channels:%d\r\n", usb->Host_channels);
    cdcprintf("hpcd_USB_OTG_FS.Init.dma_enable:%d\r\n", usb->dma_enable);
    cdcprintf("hpcd_USB_OTG_FS.Init.speed:%d\r\n", usb->speed);
    cdcprintf("hpcd_USB_OTG_FS.Init.ep0_mps:%d\r\n", usb->ep0_mps);
    cdcprintf("hpcd_USB_OTG_FS.Init.phy_itface:%d\r\n", usb->phy_itface);
    cdcprintf("hpcd_USB_OTG_FS.Init.Sof_enable:%d\r\n", usb->Sof_enable);
    cdcprintf("hpcd_USB_OTG_FS.Init.low_power_enable:%d\r\n", usb->low_power_enable);
    cdcprintf("hpcd_USB_OTG_FS.Init.lpm_enable:%d\r\n", usb->lpm_enable);
    cdcprintf("hpcd_USB_OTG_FS.Init.battery_charging_enable:%d\r\n", usb->battery_charging_enable);
    cdcprintf("hpcd_USB_OTG_FS.Init.vbus_sensing_enable:%d\r\n", usb->vbus_sensing_enable);
    cdcprintf("hpcd_USB_OTG_FS.Init.use_dedicated_ep1:%d\r\n", usb->use_dedicated_ep1);
    cdcprintf("hpcd_USB_OTG_FS.Init.use_external_vbus:%d\r\n", usb->use_external_vbus);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  HAL_GPIO_WritePin(BUZZ_GPIO_Port, BUZZ_Pin, GPIO_PIN_SET);

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
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */

  HAL_PCD_RegisterCallback(&hpcd_USB_OTG_FS, HAL_PCD_CONNECT_CB_ID,  My_PCD_ConnectCallback);
  HAL_PCD_RegisterCallback(&hpcd_USB_OTG_FS, HAL_PCD_DISCONNECT_CB_ID,  My_PCD_DisconnectCallback);
  HAL_PCD_RegisterCallback(&hpcd_USB_OTG_FS, HAL_PCD_SOF_CB_ID,  My_PCD_SOF);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  HAL_GPIO_WritePin(BUZZ_GPIO_Port, BUZZ_Pin, GPIO_PIN_SET);

  // cdcprintf("STEPPER %d\r\n", 2024);

  // HAL_Delay(300);

  debugStruc();

  while (1)
  {
      // BKPT;
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // morse("CQ ");
    // oldSofCnt = sofCnt;
    HAL_Delay(1000);
    // uint32_t periodsDiff = sofCnt-oldSofCnt;
    // cdcprintf("\t%d\t%d\r\n ", sofCnt, periodsDiff);
    debugStruc();
    HAL_Delay(1000);
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 12;
  RCC_OscInitStruct.PLL.PLLN = 96;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_USER_GPIO_Port, LED_USER_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, PULSE_Pin|DIR_Pin|BUZZ_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_USER_Pin */
  GPIO_InitStruct.Pin = LED_USER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_USER_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ES_L_Pin ES_R_Pin BUTT_JOGL_Pin BUTT_JOGR_Pin */
  GPIO_InitStruct.Pin = ES_L_Pin|ES_R_Pin|BUTT_JOGL_Pin|BUTT_JOGR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : BUTT_STEPL_Pin BUTT_STEPR_Pin */
  GPIO_InitStruct.Pin = BUTT_STEPL_Pin|BUTT_STEPR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PULSE_Pin DIR_Pin BUZZ_Pin */
  GPIO_InitStruct.Pin = PULSE_Pin|DIR_Pin|BUZZ_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

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
    HAL_GPIO_WritePin(LED_USER_GPIO_Port, LED_USER_Pin, GPIO_PIN_RESET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(LED_USER_GPIO_Port, LED_USER_Pin, GPIO_PIN_SET);
    HAL_Delay(50);
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
