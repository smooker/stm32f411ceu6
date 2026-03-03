#include "stepper.h"
#include <stdio.h>
#include <string.h>

/* replace fabsf */
#define FABS(x)     ((x) < 0.0f ? -(x) : (x))

/* replace roundf */
#define ROUND(x)    ((int32_t)((x) >= 0.0f ? (x) + 0.5f : (x) - 0.5f))

/* ---- extern motorParams from main.c ------------------------------------ */
extern params_t motorParams;

/* ---- Timer handle ------------------------------------------------- */
static TIM_HandleTypeDef *stepTim;

/* ---- State -------------------------------------------------------- */
static volatile StepperState stepperState = STEPPER_IDLE;
static volatile int32_t  stepsRemaining   = 0;
static volatile int32_t  stepCount        = 0;
static volatile int32_t  decelSteps       = 0;
static volatile uint32_t currentPeriod    = 0;
static volatile uint32_t minPeriod        = 0;   /* ticks at mmpsmax  */
static volatile uint32_t maxPeriod        = 0;   /* ticks at mmpsmin  */


static volatile int32_t decelCount = 0;

/* ---- Helpers ------------------------------------------------------ */

static uint32_t MmpsToTicks(float mmps)
{
    if (mmps <= 0.0f) return 0;
    float sps = mmps * (float)motorParams.spmm.u;   /* steps per second   */
    return (uint32_t)((float)STEPPER_TIM_CLOCK / sps);
}

static int32_t MmToSteps(float mm)
{
    return ROUND(mm * (float)motorParams.spmm.u);
}

static int32_t CalcDecelSteps(uint32_t fromPeriod, uint32_t toPeriod,
                               float decel_mmps2)
{
    float decel_sps2 = decel_mmps2 * (float)motorParams.spmm.u;
    float v0 = (float)STEPPER_TIM_CLOCK / (float)fromPeriod;
    float v1 = (float)STEPPER_TIM_CLOCK / (float)toPeriod;
    float diff = v0 * v0 - v1 * v1;
    return (int32_t)(FABS(diff) / (2.0f * decel_sps2));
}

/* ---- EEPROM ------------------------------------------------------- */

void Stepper_LoadParams(void)
{
    uint32_t val;

    /* floats stored as raw uint32_t via union — read directly         */
    if (EEPROM_Read(EE_ADDR_MMPSMAX,  &val) == EEPROM_OK) motorParams.mmpsmax.u  = val;
    else motorParams.mmpsmax.f  = DEFAULT_MMPSMAX;

    if (EEPROM_Read(EE_ADDR_MMPSMIN,  &val) == EEPROM_OK) motorParams.mmpsmin.u  = val;
    else motorParams.mmpsmin.f  = DEFAULT_MMPSMIN;

    if (EEPROM_Read(EE_ADDR_DVDTACC,  &val) == EEPROM_OK) motorParams.dvdtacc.u  = val;
    else motorParams.dvdtacc.f  = DEFAULT_DVDTACC;

    if (EEPROM_Read(EE_ADDR_DVDTDECC, &val) == EEPROM_OK) motorParams.dvdtdecc.u = val;
    else motorParams.dvdtdecc.f = DEFAULT_DVDTDECC;

    if (EEPROM_Read(EE_ADDR_JOGMM,    &val) == EEPROM_OK) motorParams.jogmm.u    = val;
    else motorParams.jogmm.f    = DEFAULT_JOGMM;

    if (EEPROM_Read(EE_ADDR_STEPMM,   &val) == EEPROM_OK) motorParams.stepmm.u   = val;
    else motorParams.stepmm.f   = DEFAULT_STEPMM;

    if (EEPROM_Read(EE_ADDR_SPMM,     &val) == EEPROM_OK) motorParams.spmm.u     = val;
    else motorParams.spmm.u     = DEFAULT_SPMM;
}

void Stepper_SaveParams(void)
{
    EEPROM_Write(EE_ADDR_MMPSMAX,  motorParams.mmpsmax.u);
    EEPROM_Write(EE_ADDR_MMPSMIN,  motorParams.mmpsmin.u);
    EEPROM_Write(EE_ADDR_DVDTACC,  motorParams.dvdtacc.u);
    EEPROM_Write(EE_ADDR_DVDTDECC, motorParams.dvdtdecc.u);
    EEPROM_Write(EE_ADDR_JOGMM,    motorParams.jogmm.u);
    EEPROM_Write(EE_ADDR_STEPMM,   motorParams.stepmm.u);
    EEPROM_Write(EE_ADDR_SPMM,     motorParams.spmm.u);
    printf("params saved\r\n");
}

void Stepper_DumpParams(void)
{
    printf("-------------------------------\r\n");
    printf("mmpsmax........: %7.3f mm/s\r\n",   motorParams.mmpsmax.f);
    printf("mmpsmin........: %7.3f mm/s\r\n",   motorParams.mmpsmin.f);
    printf("dvdtacc........: %7.3f mm/s2\r\n",  motorParams.dvdtacc.f);
    printf("dvdtdecc.......: %7.3f mm/s2\r\n",  motorParams.dvdtdecc.f);
    printf("jogmm..........: %7.3f mm\r\n",     motorParams.jogmm.f);
    printf("stepmm.........: %7.3f mm\r\n",     motorParams.stepmm.f);
    printf("spmm...........: %7lu steps/mm\r\n",motorParams.spmm.u);
    printf("-------------------------------\r\n");
    printf("pulse_ticks....: %lu\r\n", (uint32_t)PULSE_TICKS);
    printf("min_period.....: %lu ticks (%.1f mm/s)\r\n",
              MmpsToTicks(motorParams.mmpsmax.f), motorParams.mmpsmax.f);
    printf("max_period.....: %lu ticks (%.1f mm/s)\r\n",
              MmpsToTicks(motorParams.mmpsmin.f), motorParams.mmpsmin.f);
    printf("-------------------------------\r\n");
}

void Stepper_SetParam(const char *name, float value)
{
    if      (strcmp(name, "mmpsmax")  == 0) motorParams.mmpsmax.f  = value;
    else if (strcmp(name, "mmpsmin")  == 0) motorParams.mmpsmin.f  = value;
    else if (strcmp(name, "dvdtacc")  == 0) motorParams.dvdtacc.f  = value;
    else if (strcmp(name, "dvdtdecc") == 0) motorParams.dvdtdecc.f = value;
    else if (strcmp(name, "jogmm")    == 0) motorParams.jogmm.f    = value;
    else if (strcmp(name, "stepmm")   == 0) motorParams.stepmm.f   = value;
    else if (strcmp(name, "spmm")     == 0) motorParams.spmm.u     = (uint32_t)value;
    else { printf("unknown param: %s\r\n", name); return; }
    printf("%s = %.3f\r\n", name, value);
}

/* ---- Init --------------------------------------------------------- */

void Stepper_Init(TIM_HandleTypeDef *htim)
{
    stepTim = htim;

    /* ensure DIR and PULSE pins are low at start */
    HAL_GPIO_WritePin(PULSE_GPIO_Port, PULSE_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DIR_GPIO_Port,   DIR_Pin,   GPIO_PIN_RESET);

    /* fixed pulse width */
    __HAL_TIM_SET_COMPARE(stepTim, TIM_CHANNEL_3, PULSE_TICKS);

    printf("stepper init ok\r\n");
    Stepper_DumpParams();
}

/* ---- Move --------------------------------------------------------- */

static void StartMove(int32_t steps)
{
    if (stepperState != STEPPER_IDLE)
    {
        printf("stepper busy\r\n");
        return;
    }
    if (steps == 0) return;

    /* set direction */
    HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin,
                      steps > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* wait for DIR to settle BEFORE starting timer — no pulse generated */
    /* 50us at 96MHz = 4800 NOPs roughly, use DWT or simple loop         */
    uint32_t t = HAL_GetTick();
    while (HAL_GetTick() == t);  /* wait at least 1ms — more than enough */

    stepsRemaining = ABS(steps);  /* use (steps < 0) ? -steps : steps    */
    stepCount      = 0;
    minPeriod      = MmpsToTicks(motorParams.mmpsmax.f);
    maxPeriod      = MmpsToTicks(motorParams.mmpsmin.f);
    currentPeriod  = maxPeriod;

    decelSteps = CalcDecelSteps(minPeriod, maxPeriod, motorParams.dvdtdecc.f);
    if (decelSteps > stepsRemaining / 2)
        decelSteps = stepsRemaining / 2;

    stepperState = STEPPER_ACCEL;  /* go straight to ACCEL — no DIRSETUP state needed */

    __HAL_TIM_SET_AUTORELOAD(stepTim, currentPeriod - 1);
    __HAL_TIM_SET_COMPARE(stepTim, TIM_CHANNEL_3, PULSE_TICKS);
    __HAL_TIM_SET_COUNTER(stepTim, 0);
    HAL_TIM_PWM_Start_IT(stepTim, TIM_CHANNEL_3);

    printf("move %ld steps\r\n", (int32_t)(steps < 0 ? -steps : steps));

    printf("minPeriod: %lu maxPeriod: %lu\r\n", minPeriod, maxPeriod);
    printf("decelSteps: %ld stepsRemaining: %ld\r\n", decelSteps, stepsRemaining);
    printf("accelSteps needed: ~%ld\r\n", decelSteps); // same as decel for equal accel/decel
}

void Stepper_Move(float mm)
{
    int32_t steps = MmToSteps(mm);
    printf("move %.3f mm -> %ld steps\r\n", mm, steps);
    StartMove(steps);
}

void Stepper_MoveSteps(int32_t steps)
{
    printf("move %ld steps\r\n", steps);
    StartMove(steps);
}

void Stepper_Jog(float mm)
{
    /* jog uses jogmm as unit — positive = right, negative = left      */
    float dist = (mm >= 0.0f ? 1.0f : -1.0f) * motorParams.jogmm.f;
    int32_t steps = MmToSteps(dist);
    printf("jog %.3f mm -> %ld steps\r\n", dist, steps);
    StartMove(steps);
}

void Stepper_Stop(void)
{
    if (stepperState != STEPPER_IDLE)
    {
        stepperState = STEPPER_DECEL;
        printf("stopping...\r\n");
    }
}

uint8_t Stepper_IsBusy(void)
{
    return stepperState != STEPPER_IDLE;
}


static int32_t PeriodToIndex(uint32_t period, float decel_mmps2)
{
    /* reverse the Linstedt formula — find n from current period */
    float decel_sps2 = decel_mmps2 * (float)motorParams.spmm.u;
    float v = (float)STEPPER_TIM_CLOCK / (float)period;
    return (int32_t)(v * v / (2.0f * decel_sps2));
}

void Stepper_StartDecel(void)
{
    if (stepperState == STEPPER_IDLE) return;
    decelCount   = PeriodToIndex(currentPeriod, motorParams.dvdtdecc.f);
    stepperState = STEPPER_DECEL;
}

/* ---- ISR ---------------------------------------------------------- */
/* Call from HAL_TIM_PWM_PulseFinishedCallback                         */

void Stepper_ISR(void)
{
    if (stepperState == STEPPER_DIRSETUP)
    {
        /* DIR setup delay done — now load first real period */
        /* stop PWM, reconfigure, restart so no pulse fired during setup */
        HAL_TIM_PWM_Stop_IT(stepTim, TIM_CHANNEL_3);
        stepperState = STEPPER_ACCEL;
        __HAL_TIM_SET_AUTORELOAD(stepTim, currentPeriod - 1);
        __HAL_TIM_SET_COMPARE(stepTim, TIM_CHANNEL_3, PULSE_TICKS);
        __HAL_TIM_SET_COUNTER(stepTim, 0);
        HAL_TIM_PWM_Start_IT(stepTim, TIM_CHANNEL_3);
        return;
    }

    stepsRemaining--;
    stepCount++;

    if (stepsRemaining == 0)
    {
        /* wait for current pulse to finish (counter > CCR) */
        while (__HAL_TIM_GET_COUNTER(stepTim) < __HAL_TIM_GET_COMPARE(stepTim, TIM_CHANNEL_3));
        HAL_TIM_PWM_Stop_IT(stepTim, TIM_CHANNEL_3);
        stepperState = STEPPER_IDLE;
        return;
    }

    switch (stepperState)
    {
    case STEPPER_ACCEL:
        currentPeriod -= (2 * currentPeriod) / (4 * stepCount + 1);
        if (currentPeriod <= minPeriod)
        {
            currentPeriod = minPeriod;
            stepperState  = STEPPER_CONST;
        }
        else if (stepsRemaining <= decelSteps)
        {
            /* max speed not reached — start decel from current speed */
            decelCount   = PeriodToIndex(currentPeriod, motorParams.dvdtdecc.f);
            stepperState = STEPPER_DECEL;
        }
        break;

    case STEPPER_CONST:
        if (stepsRemaining <= decelSteps)
        {
            /* reached decel point — start decel from max speed */
            decelCount   = PeriodToIndex(minPeriod, motorParams.dvdtdecc.f);
            stepperState = STEPPER_DECEL;
        }
        break;

    case STEPPER_DECEL:
        currentPeriod += (2 * currentPeriod) / (4 * decelCount + 1);
        decelCount--;
        if (currentPeriod >= maxPeriod || decelCount <= 0)
            currentPeriod = maxPeriod;
        break;

    default:
        break;
    }

    __HAL_TIM_SET_AUTORELOAD(stepTim, currentPeriod - 1);
    __HAL_TIM_SET_COMPARE(stepTim, TIM_CHANNEL_1, PULSE_TICKS);
}
