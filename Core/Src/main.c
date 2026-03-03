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

union {
    float f;
    uint32_t u;
} float2uint;

typedef struct
{
  // float mmpsmax;          // velocity maximum
  union {
      float f;
      uint32_t u;
  } mmpsmax;

  // float mmpsmin;          // velocity minimum
  union {
      float f;
      uint32_t u;
  } mmpsmin;

  // float dvdtacc;          // acceleration
  union {
      float f;
      uint32_t u;
  } dvdtacc;

  // float dvdtdecc;         // decceleration
  union {
      float f;
      uint32_t u;
  } dvdtdecc;

  // float jogmm;            // jog units
  union {
      float f;
      uint32_t u;
  } jogmm;

  // float stepmm;           // step units
  union {
      float f;
      uint32_t u;
  } stepmm;

  // uint32_t spmm;          // steps per mm (conversational unit)
  union {
      float f;
      uint32_t u;
  } spmm;
} params_t;

typedef enum {
    KEY_NONE,
    KEY_SEQUENCE,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_ESC,
    KEY_ENTER,
    KEY_BACKSPACE,
    KEY_CLEAR,
} KeyCode;

typedef enum {
    SEQ_IDLE,
    SEQ_ESC,
    SEQ_BRACKET,
} SeqState;

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
uint8_t CDC_TxWrite(const uint8_t *data, uint16_t len);

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

//Externs
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
extern USBD_HandleTypeDef hUsbDeviceFS;

//
#define RX_BUF_SIZE  256
static uint8_t UserTxBufferFS[RX_BUF_SIZE];
static volatile uint16_t txLen   = 0;
static volatile uint8_t  txBusy  = 0;

//
#define TX_BUF_SIZE  256
static uint8_t UserRxBufferFS[TX_BUF_SIZE];
static volatile uint16_t rxHead = 0;
static volatile uint16_t rxTail = 0;
//
extern uint8_t CDC_IsConnected;

// Locals
params_t params;

static uint8_t  lineBuf[128];
static uint16_t lineLen = 0;

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

//
KeyCode ParseKey(uint8_t byte)
{
    static SeqState state = SEQ_IDLE;

    switch (state)
    {
    case SEQ_IDLE:
        if      (byte == 0x1B) { state = SEQ_ESC;      return KEY_SEQUENCE; }
        else if (byte == '\r' || byte == '\n')         return KEY_ENTER;
        else if (byte == 0x7F || byte == 0x08)         return KEY_BACKSPACE;
        else if (byte == 0x0C) {
            return KEY_CLEAR;  // Ctrl+L
        }
        break;

    case SEQ_ESC:
        if (byte == '[') { state = SEQ_BRACKET; return KEY_SEQUENCE; }
        state = SEQ_IDLE;
        return KEY_ESC;

    case SEQ_BRACKET:
        state = SEQ_IDLE;
        if      (byte == 'A') return KEY_UP;
        else if (byte == 'B') return KEY_DOWN;
        else if (byte == 'C') return KEY_RIGHT;
        else if (byte == 'D') return KEY_LEFT;
        break;
    }

    return KEY_NONE;
}

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
  params.mmpsmax.f  = 1.0012f;    // 1 index
  params.mmpsmin.f  = 1.0023f;    // 2
  params.dvdtacc.f  = 1.0034f;    // 3
  params.dvdtdecc.f = 1.0045f;    // 4
  params.jogmm.f    = 1.0056f;    // 5
  params.stepmm.f   = 1.0067f;    // 6
  params.spmm.u     = 4096;       // 7
}

// debug only
void writeParams()
{
  int16_t index = 0;

  index++;
  if (EEPROM_Write(index, params.mmpsmax.u) != EEPROM_OK) {
    cdcprintf("WRITE 1 FAILED\r\n");
    BKPT;
  }
  index++;
  if (EEPROM_Write(index, params.mmpsmin.u) != EEPROM_OK) {
    cdcprintf("WRITE 2 FAILED\r\n");
    BKPT;
  }
  index++;
  if (EEPROM_Write(index, params.dvdtacc.u) != EEPROM_OK) {
    cdcprintf("WRITE 3 FAILED\r\n");
    BKPT;
  }
  index++;
  if (EEPROM_Write(index, params.dvdtdecc.u) != EEPROM_OK) {
    cdcprintf("WRITE 4 FAILED\r\n");
    BKPT;
  }
  index++;
  if (EEPROM_Write(index, params.jogmm.u) != EEPROM_OK) {
    cdcprintf("WRITE 5 FAILED\r\n");
    BKPT;
  }
  index++;
  if (EEPROM_Write(index, params.stepmm.u) != EEPROM_OK) {
    cdcprintf("WRITE 6 FAILED\r\n");
    BKPT;
  }
  index++;
  if (EEPROM_Write(index, params.spmm.u) != EEPROM_OK) {
    cdcprintf("WRITE 7 FAILED\r\n");
    BKPT;
  }
}

//
void readParams()
{
    int16_t stat;
    uint16_t index = 0;

    index++;
    if ( (stat = EEPROM_Read(index, (uint32_t*)&params.mmpsmax)) != 0) {
        cdcprintf("read %d returned 0x%x\r\n", index, stat);
    }

    index++;
    if ( (stat = EEPROM_Read(index, (uint32_t*)&params.mmpsmin)) != 0) {
        cdcprintf("read %d returned 0x%x\r\n", index, stat);
    }

    index++;
    if ( (stat = EEPROM_Read(index, (uint32_t*)&params.dvdtacc)) != 0) {
        cdcprintf("read %d returned 0x%x\r\n", index, stat);
    }

    index++;
    if ( (stat = EEPROM_Read(index, (uint32_t*)&params.dvdtdecc)) != 0) {
        cdcprintf("read %d returned 0x%x\r\n", index, stat);
    }

    index++;
    if ( (stat = EEPROM_Read(index, (uint32_t*)&params.jogmm)) != 0) {
        cdcprintf("read %d returned 0x%x\r\n", index, stat);
    }

    index++;
    if ( (stat = EEPROM_Read(index, (uint32_t*)&params.stepmm)) != 0) {
        cdcprintf("read %d returned 0x%x\r\n", index, stat);
    }

    index++;
    if ( (stat = EEPROM_Read(index, (uint32_t*)&params.spmm)) != 0) {
        cdcprintf("read %d returned 0x%x\r\n", index, stat);
    }
}

//
void dumpVars()
{
    // readVariables();
    // cdcprintf("----------%08d-----\r\n", debugonly++);
    cdcprintf("Dump of NVARS in EEPROM\r\n");
    cdcprintf("-----------------------\r\n");
    cdcprintf("mmpsmax........: %7.3f\r\n", params.mmpsmax.f );
    cdcprintf("mmpsmin........: %7.3f\r\n", params.mmpsmin.f );
    cdcprintf("dvdtacc........: %7.3f\r\n", params.dvdtacc.f );
    cdcprintf("dvdtdecc.......: %7.3f\r\n", params.dvdtdecc.f );
    cdcprintf("jogmm..........: %7.3f\r\n", params.jogmm.f );
    cdcprintf("stepmm.........: %7.3f\r\n", params.stepmm.f );
    cdcprintf("spmm...........: %7d\r\n",  params.spmm.u );
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

int _write(int file, char *ptr, int len)
{
    UNUSED(file);

    USBD_CDC_HandleTypeDef *hcdc =
        (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;

    /* wait until TX is free */
    uint32_t timeout = HAL_GetTick();
    while (hcdc->TxState != 0)
    {
        if (HAL_GetTick() - timeout > 100)  /* 100ms timeout — avoid infinite hang */
            return 0;
    }

    CDC_Transmit_FS((uint8_t *)ptr, len);
    return len;
}

// console stdout
uint8_t cdcprintf(const char *format, ... )
{
    uint8_t buffx[256] = {0};

    if (!CDC_IsConnected) {
        // BKPT;
        return 1;
    }

    uint8_t result = USBD_FAIL;

    va_list ap;

    int vsprintfResult;

    va_start(ap, format);
    vsprintfResult = vsprintf((char *)&buffx[0], format, ap);
    if ( vsprintfResult < 0 ) {
        BKPT;
    }
    va_end(ap);

    uint8_t len = strlen((const char*)&buffx);
    CDC_TxWrite(buffx, len);

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

//
static void TxStart(void)
{
    if (txBusy || txLen == 0) return;
    txBusy = 1;
    /* txLen intentionally NOT cleared here — buffer must stay valid */
    uint8_t result = CDC_Transmit_FS(UserTxBufferFS, txLen);
    if (result == USBD_OK)
    {
        txBusy = 1;
        txLen  = 0;
    }
}

//
void MyCDC_Receive_FS(uint8_t *Buf, uint32_t *Len)
{
    for (uint32_t i = 0; i < *Len; i++)
    {
        uint16_t next = (rxHead + 1) % RX_BUF_SIZE;
        if (next != rxTail)
        {
            UserRxBufferFS[rxHead] = Buf[i];
            rxHead = next;
        }
    }
}

/* Read one byte from RX buffer. Call CDC_RxAvailable() first. */
uint8_t CDC_RxAvailable(void)
{
    return rxHead != rxTail;
}

uint8_t CDC_RxRead(void)
{
    uint8_t byte = UserRxBufferFS[rxTail];
    rxTail = (rxTail + 1) % APP_RX_DATA_SIZE;
    return byte;
}

/* Queue bytes for async TX. Returns 0 if TX buffer full. */
uint8_t CDC_TxWrite(const uint8_t *data, uint16_t len)
{
    if (txBusy || (txLen + len) > APP_TX_DATA_SIZE)
        return 0; /* busy or won't fit */

    memcpy(&UserTxBufferFS[txLen], data, len);
    txLen += len;
    TxStart();
    return 1;
}

// DataOut is from HOST to DEVICE
static void MyPCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_DataOutStage((USBD_HandleTypeDef *)hpcd->pData, epnum,
                         hpcd->OUT_ep[epnum].xfer_buff);
}

// DataIn is from DEVICE to HOST
void MyPCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_DataInStage((USBD_HandleTypeDef *)hpcd->pData, epnum,
                        hpcd->IN_ep[epnum].xfer_buff);

    if (epnum == 0x01)
    {
        txLen  = 0;   // ← clear here, AFTER hardware is done with the buffer
        txBusy = 0;
        TxStart();    // send next chunk if queued in the meantime
    }
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

void ProcessLine(uint8_t *buf, uint16_t len)
{
    uint8_t resp[128];
    uint16_t pos = 0;

    memcpy(&resp[pos], " Got: ", 5);  pos += 5;
    memcpy(&resp[pos], buf, len);    pos += len;
    memcpy(&resp[pos], "\r\n", 2);   pos += 2;

    CDC_Transmit_FS(resp, pos);
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

  setvbuf(stdout, NULL, _IONBF, 0);  /* disable buffering entirely */

  HAL_PCD_RegisterCallback(&hpcd_USB_OTG_FS, HAL_PCD_CONNECT_CB_ID,  My_PCD_ConnectCallback);
  HAL_PCD_RegisterCallback(&hpcd_USB_OTG_FS, HAL_PCD_DISCONNECT_CB_ID,  My_PCD_DisconnectCallback);
  HAL_PCD_RegisterCallback(&hpcd_USB_OTG_FS, HAL_PCD_SOF_CB_ID,  My_PCD_SOF);

  // FROM HOST TO DEVICE
  HAL_PCD_RegisterDataOutStageCallback(&hpcd_USB_OTG_FS, MyPCD_DataOutStageCallback);
  // FROM DEVICE TO HOST
  HAL_PCD_RegisterDataInStageCallback(&hpcd_USB_OTG_FS, MyPCD_DataInStageCallback);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  // silence buzzer
  HAL_GPIO_WritePin(BUZZ_GPIO_Port, BUZZ_Pin, GPIO_PIN_SET);

  if ( EEPROM_Init() != EEPROM_OK ) {
      BKPT;
  }

  // USB enumeration
  HAL_Delay(1200);

  // initParams();
  // writeParams();
  // BKPT;
  readParams();
  // BKPT;

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* Drain RX buffer */
    while (CDC_RxAvailable())
    {
        uint8_t b = CDC_RxRead();

        KeyCode key = ParseKey(b);

        switch (key)
        {
        case KEY_UP:
            /* handle up    */
            printf("KEY_UP pressed.\r\n");
            break;
        case KEY_DOWN:
            /* handle down  */
            printf("KEY_DOWN pressed.\r\n");
            break;
        case KEY_LEFT:
            /* handle left  */
            printf("KEY_LEFT pressed.\r\n");
            break;
        case KEY_RIGHT:
            /* handle right */
            printf("KEY_RIGHT pressed.\r\n");
            break;
        case KEY_ENTER:
            /* handle enter */
            if (lineLen > 0)
            {
                lineBuf[lineLen] = '\0';  /* null terminate for sscanf */
                ProcessLine(lineBuf, lineLen);
                lineLen = 0;
            }
            break;
        case KEY_BACKSPACE:
            if (lineLen > 0)
            {
                lineLen--;
                printf("\b \b");
            }
            break;
        case KEY_CLEAR:
            printf("\033[2J\033[H> %.*s", lineLen, lineBuf);
            break;
        case KEY_NONE:
            /* KEY_SEQUENCE is ignored, only true printable bytes reach here */
            if (b >= 0x20 && b < 0x7F && lineLen < sizeof(lineBuf) - 1)
            {
                lineBuf[lineLen++] = b;
                printf("%c", b);
            }
            break;

        default: break;
        }
    }
    // TxStart(); // keep retrying until it goes through

    // morse("C");
    // if (CDC_RxAvailable()) {
    //     cdcprintf("neshto doide po zhicata....\r\n");
    // }
    // debugStruc();
    // dumpVars();
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
