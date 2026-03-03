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
#include "eeprom_emul_uint32_t.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef struct
{
  float mmpsmax;          // velocity maximum
  float mmpsmin;          // velocity minimum
  float dvdtacc;          // acceleration
  float dvdtdecc;         // decceleration
  float jogmm;            // jog units
  float stepmm;           // step units
  uint32_t spmm;          // steps per mm (conversational unit)
} params_t;

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

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

//Externs
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
extern USBD_HandleTypeDef hUsbDeviceFS;
extern uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];
extern uint8_t CDC_IsConnected;

// Locals
uint8_t TCFlag = 0;        //Transfer Complete Flag for usb cdcprintf
params_t params;

//
uint32_t semaphore = 0;    // state machine flags

// morse code letters and digits
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

// my memset
void volatile_memset(volatile void *s, int c, size_t n) {
    volatile unsigned char *p = (volatile unsigned char *)s;
    while (n-- > 0) {
        *p++ = (unsigned char)c;
    }
}

// parameters init - for debug purposes only
void initParams()
{
  params.mmpsmax  = 1.0012f;
  params.mmpsmin  = 1.0023f;
  params.dvdtacc  = 1.0034f;
  params.dvdtdecc = 1.0045f;
  params.jogmm    = 1.0056f;
  params.stepmm   = 1.0067f;
  params.spmm     = 4096;
}

void dumpVars()
{
    // readVariables();
    // cdcprintf("----------%08d-----\r\n", debugonly++);
    cdcprintf("Dump of NVARS in EEPROM\r\n");
    cdcprintf("-----------------------\r\n");
    cdcprintf("mmpsmax........: %7.3f\r\n", params.mmpsmax );
    cdcprintf("mmpsmin........: %7.3f\r\n", params.mmpsmin );
    cdcprintf("dvdtacc........: %7.3f\r\n", params.dvdtacc );
    cdcprintf("dvdtdecc.......: %7.3f\r\n", params.dvdtdecc );
    cdcprintf("jogmm..........: %7.3f\r\n", params.jogmm );
    cdcprintf("stepmm.........: %7.3f\r\n", params.stepmm );
    cdcprintf("spmm...........: %7d\r\n",  params.spmm );
    cdcprintf("-----------------------\r\n");
    cdcprintf("semaphore....:  %d\r\n", semaphore);
    cdcprintf("SEM_EL.......:  %d\r\n", SEM_EL);
    cdcprintf("SEM_ER.......:  %d\r\n", SEM_ER);
    cdcprintf("SEM_JOGL.....:  %d\r\n", SEM_JOGL);
    cdcprintf("SEM_JOGR.....:  %d\r\n", SEM_JOGR);
    cdcprintf("SEM_JOGSTEPL.:  %d\r\n", SEM_JOGSTEPL);
    cdcprintf("SEM_JOGSTEPR.:  %d\r\n", SEM_JOGSTEPR);
    cdcprintf("-----------------------\r\n");
    cdcprintf("DB_JOGL......:  %d\r\n", DB_JOGL);
    cdcprintf("DB_JOGR......:  %d\r\n", DB_JOGR);
    cdcprintf("DB_STEPL.....:  %d\r\n", DB_STEPL);
    cdcprintf("DB_STEPR.....:  %d\r\n", DB_STEPR);
    cdcprintf("DB_EL........:  %d\r\n", DB_EL);
    cdcprintf("DB_ER........:  %d\r\n", DB_ER);
    cdcprintf("DB_EE........:  %d\r\n", DB_EE);
    cdcprintf("-----------------------\r\n");
}

// morse dot function
void dot()
{
    HAL_GPIO_WritePin(LED_USER_GPIO_Port, LED_USER_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BUZZ_GPIO_Port, BUZZ_Pin, GPIO_PIN_RESET);

    HAL_Delay(dotTime);

    HAL_GPIO_WritePin(LED_USER_GPIO_Port, LED_USER_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(BUZZ_GPIO_Port, BUZZ_Pin, GPIO_PIN_SET);

    HAL_Delay(interTime);
}

// morse dash function
void dash()
{
    HAL_GPIO_WritePin(LED_USER_GPIO_Port, LED_USER_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BUZZ_GPIO_Port, BUZZ_Pin, GPIO_PIN_RESET);

    HAL_Delay(dashTime);

    HAL_GPIO_WritePin(BUZZ_GPIO_Port, BUZZ_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_USER_GPIO_Port, LED_USER_Pin, GPIO_PIN_SET);

    HAL_Delay(interTime);
}

// console stdout
uint8_t cdcprintf(const char *format, ... )
{
    if (!CDC_IsConnected) {
        // BKPT;
        return 1;
    }

    uint8_t result = USBD_FAIL;

    va_list ap;

    // volatile_memset((volatile void*)UserTxBufferFS, 0, sizeof(UserTxBufferFS));

    int vsprintfResult;

    va_start(ap, format);
    vsprintfResult = vsprintf((char *)&UserTxBufferFS[0], format, ap);
    if ( vsprintfResult < 0 ) {
        BKPT;
    }
    va_end(ap);
    uint8_t len = strlen((const char*)&UserTxBufferFS);

    while ( (result != USBD_OK) ) {
        result = CDC_Transmit_FS(&UserTxBufferFS[0], (uint16_t)len);
    }

    return result; //
}

// Morse code transmitter
uint8_t morse(const char *format, ... )
{
    uint8_t result = 0;

    unsigned char lettInMorse[8];

    uint8_t cnt2 = 0;

    uint8_t symbol;
    // uint8_t tmpsymbol;           //needed for CW printout in console

    while ( (symbol = format[cnt2]) > 0)
    {
        // end of string
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

        // convert to pseudoascii
        if ( ( symbol <= 57 ) & ( symbol >= 48 ) ) {
          symbol -= 48;
        }

        // convert to pseudoascii
        if ( ( symbol <= 90 ) & ( symbol >= 65 ) ) {
          symbol -= 55;
        }
        // space handling
        if (symbol == 32) {
            HAL_Delay(spaceTime);
            cnt2++;
            continue;
        }

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
    return result;
}

//
void My_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
    UNUSED(hpcd);
    BKPT;
}

// USB Physical connection
void My_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
    UNUSED(hpcd);
}

// USB Physical disconnection
void My_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
    UNUSED(hpcd);
    CDC_IsConnected = 0;
}

// Many nops
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

// Start Of Frame Handling.
void My_PCD_SOF(PCD_HandleTypeDef *hpcd)
{
    UNUSED(hpcd);
    //fixme. find out why there are two sof needles
    // HAL_GPIO_WritePin(LED_USER_GPIO_Port, LED_USER_Pin, GPIO_PIN_RESET);
    // delay();
    // HAL_GPIO_WritePin(LED_USER_GPIO_Port, LED_USER_Pin, GPIO_PIN_SET);
}

// DataOut is from HOST to DEVICE
static void MyPCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    uint32_t pcount = HAL_PCD_EP_GetRxCount(hpcd, epnum);
    UNUSED(pcount);

    if (epnum == 0x01)
    {
        // Process_My_Custom_Data(hpcd->OUT_ep[epnum].xfer_buff, pcount);
        HAL_PCD_EP_Receive(hpcd, epnum, hpcd->OUT_ep[epnum].xfer_buff, hpcd->OUT_ep[epnum].xfer_len);
        // BKPT;       //during setup and later
    }
    else
    {
        USBD_LL_DataOutStage((USBD_HandleTypeDef*)hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff);
    }
}

// DataIn is from DEVICE to HOST
void MyPCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    UNUSED(hpcd);

    if (epnum == 0x01) {
        TCFlag = 1;
    }
    USBD_LL_DataInStage((USBD_HandleTypeDef*)hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff);
}

// Debug some USB options
void debugStruc()
{
    USB_CfgTypeDef *usb = &hpcd_USB_OTG_FS.Init;
    cdcprintf("#########################################\r\n");
    cdcprintf("sizeof float is %d bytes\r\n", sizeof(float));
    cdcprintf("-----------------------------------------\r\n");
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

  // fire up buzzer for diagnostics. before main we are silencing it
  HAL_GPIO_WritePin(BUZZ_GPIO_Port, BUZZ_Pin, GPIO_PIN_RESET);

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
  HAL_PCD_RegisterDataOutStageCallback(&hpcd_USB_OTG_FS, MyPCD_DataOutStageCallback);
  HAL_PCD_RegisterDataInStageCallback(&hpcd_USB_OTG_FS, MyPCD_DataInStageCallback);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  // silence buzzer
  HAL_GPIO_WritePin(BUZZ_GPIO_Port, BUZZ_Pin, GPIO_PIN_SET);

  if ( EEPROM_Init() != EEPROM_OK ) {
      BKPT;
  }

  initParams();

  // USB enumeration
  HAL_Delay(1200);

  uint16_t asdf = 0x55aa;

  if ( (asdf = EEPROM_Write(0x0001, 0x55aa50a0)) != 0x00 ) {
      BKPT;
  }

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    // SANDBOX

    morse("C");
    // debugStruc();
    dumpVars();
    // HAL_Delay(500);
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
