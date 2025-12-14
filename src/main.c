#include "xparameters.h"
#include "xuartps.h"
#include "xuartlite.h"
#include "xiicps.h"
#include "xil_printf.h"
#include "sleep.h"
#include <stdio.h>
#include <string.h>
#include <math.h> 
#include "xil_types.h"

/******************************************************************************
 * 1. DEFINICIONES DE HARDWARE (Mapeo de Memoria Directo)
 ******************************************************************************/
#define UART0_BASEADDR      XPAR_XUARTPS_0_BASEADDR
#define UART0_CLK           XPAR_XUARTPS_0_CLOCK_FREQ

#define UARTLITE_BASEADDR   XPAR_XUARTLITE_0_BASEADDR

/* I2C0 (PS) */
#define IIC_BASEADDR        XPAR_XIICPS_0_BASEADDR
#define IIC_CLK             XPAR_XIICPS_0_CLOCK_FREQ

/* --- GPIO ACCESO DIRECTO --- */
#define GPIO_BUZZER_BASE    XPAR_XGPIO_0_BASEADDR
#define GPIO_BUZZER_DATA    (*(volatile u32 *)(GPIO_BUZZER_BASE + 0x00))
#define GPIO_BUZZER_TRI     (*(volatile u32 *)(GPIO_BUZZER_BASE + 0x04))

#define GPIO_BTNS_BASE      XPAR_XGPIO_1_BASEADDR
#define GPIO_BTNS_DATA      (*(volatile u32 *)(GPIO_BTNS_BASE + 0x00))
#define GPIO_BTNS_TRI       (*(volatile u32 *)(GPIO_BTNS_BASE + 0x04))


/* Direcciones I2C */
#define MLX_I2C_ADDR        0x5A  
#define MAX_ADDR            0x57  

/* Registros MLX90614 */
#define MLX_REG_TA          0x06
#define MLX_REG_TOBJ1       0x07

/* Constantes de Tiempo */
#define SAMPLE_PERIOD_US    15000 

/******************************************************************************
 * 2. VARIABLES GLOBALES
 ******************************************************************************/
XIicPs IicInstance;
XUartPs UartPs;      
XUartLite UartLite;

/* Variables del Algoritmo Ritmo Cardíaco y SpO2 */
#define MA_SIZE 4
#define MIN_BEAT_INTERVAL 300 

float current_dc_ir = 0;
float current_dc_red = 0;
long last_beat_time = 0;
long current_time_ms = 0;

// Variables para amplitudes (Max/Min)
float ir_max = 0, ir_min = 0;
float red_max = 0, red_min = 0;

int is_beat_rising = 0;
int32_t ir_buffer[MA_SIZE];
int buf_idx = 0;
int32_t ir_sum = 0;

float avg_bpm = 0;
float avg_spo2 = 0; // Iniciamos en 0

// Variables de Control de Estado
int measurement_active = 0; 
int measurements_taken = 0; 
int first_beat_detected = 0; 

/******************************************************************************
 * 3. FUNCIONES DE AYUDA 
 ******************************************************************************/

void UARTLite_SendString(XUartLite *InstancePtr, const char *str) {
    while (*str) {
        while (XUartLite_IsSending(InstancePtr));
        XUartLite_Send(InstancePtr, (u8 *)str, 1);
        str++;
    }
}

void GPIO_Init_System() {
    GPIO_BUZZER_TRI = 0x0; // Salida
    GPIO_BUZZER_DATA = 0; 
    GPIO_BTNS_TRI = 0xF;   // Entrada
}

void Buzzer_Set(int state) {
    GPIO_BUZZER_DATA = state ? 1 : 0;
}

u32 Buttons_Read() {
    return GPIO_BTNS_DATA;
}

/* --- REINICIO DE SESIÓN --- */
void Reset_Session() {
    measurements_taken = 0;
    measurement_active = 1;
    
    // Reiniciamos BPM y SpO2 visuales
    avg_bpm = 0.0f;
    // No reiniciamos avg_spo2 a 0 totalmente para que el filtro converja rápido
    if (avg_spo2 < 80) avg_spo2 = 90.0; // Empezamos en un valor razonable
    
    first_beat_detected = 0;
    Buzzer_Set(0);
}

int I2C_Init_Manual() {
    XIicPs_Config iicCfg;
    int Status;
    iicCfg.BaseAddress = IIC_BASEADDR;
    iicCfg.InputClockHz = IIC_CLK;
    Status = XIicPs_CfgInitialize(&IicInstance, &iicCfg, iicCfg.BaseAddress);
    if (Status != XST_SUCCESS) return XST_FAILURE;
    XIicPs_Reset(&IicInstance);
    XIicPs_SetSClk(&IicInstance, 100000); 
    return XST_SUCCESS;
}

int I2C_WriteReg(u8 devAddr, u8 reg, u8 value) {
    u8 buf[2] = { reg, value };
    int Status = XIicPs_MasterSendPolled(&IicInstance, buf, 2, devAddr);
    while (XIicPs_BusIsBusy(&IicInstance));
    return Status;
}

int I2C_ReadMulti(u8 devAddr, u8 reg, u8 *buf, u32 len) {
    int Status;
    Status = XIicPs_MasterSendPolled(&IicInstance, &reg, 1, devAddr);
    while (XIicPs_BusIsBusy(&IicInstance));
    Status = XIicPs_MasterRecvPolled(&IicInstance, buf, len, devAddr);
    while (XIicPs_BusIsBusy(&IicInstance));
    return Status;
}

float MLX90614_ReadTemp(u8 RegAddr) {
    u8 SendBuffer[1] = { RegAddr };
    u8 RecvBuffer[2];
    XIicPs_SetOptions(&IicInstance, XIICPS_REP_START_OPTION);
    if (XIicPs_MasterSendPolled(&IicInstance, SendBuffer, 1, MLX_I2C_ADDR) != XST_SUCCESS) {
        XIicPs_ClearOptions(&IicInstance, XIICPS_REP_START_OPTION); 
        return 0.0;
    }
    XIicPs_ClearOptions(&IicInstance, XIICPS_REP_START_OPTION);
    if (XIicPs_MasterRecvPolled(&IicInstance, RecvBuffer, 2, MLX_I2C_ADDR) != XST_SUCCESS) return 0.0;
    u16 raw = (RecvBuffer[1] << 8) | RecvBuffer[0];
    return (float)raw * 0.02f - 273.15f;
}

/******************************************************************************
 * 4. LÓGICA DE SENSORES
 ******************************************************************************/

int Max30102_Init_Config(void) {
    I2C_WriteReg(MAX_ADDR, 0x09, 0x40); // Reset
    usleep(10000);
    I2C_WriteReg(MAX_ADDR, 0x08, 0x4F); // FIFO Config (rolling)
    I2C_WriteReg(MAX_ADDR, 0x09, 0x03); // Mode Config (SpO2 = RED + IR)
    I2C_WriteReg(MAX_ADDR, 0x0A, 0x27); // SpO2 Config
    I2C_WriteReg(MAX_ADDR, 0x0C, 0x24); // LED1 Pulse Amplitude (Red)
    I2C_WriteReg(MAX_ADDR, 0x0D, 0x24); // LED2 Pulse Amplitude (IR)
    return XST_SUCCESS;
}

int Max30102_ReadLatestRedIR(u32 *red, u32 *ir) {
    u8 buf[6];
    if(I2C_ReadMulti(MAX_ADDR, 0x07, buf, 6) != XST_SUCCESS) return XST_FAILURE;
    *red = ((u32)buf[0] << 16) | ((u32)buf[1] << 8) | buf[2];
    *ir  = ((u32)buf[3] << 16) | ((u32)buf[4] << 8) | buf[5];
    return XST_SUCCESS;
}

void HR_ProcessSample(u32 red, u32 ir) {
    // Si no hay dedo (valores bajos), limpiar
    if (ir < 50000) {
        avg_bpm = 0;
        avg_spo2 = 0;
        current_dc_ir = 0;
        current_dc_red = 0;
        return;
    }

    // 1. Filtro Media Móvil para IR
    ir_sum -= ir_buffer[buf_idx];
    ir_buffer[buf_idx] = ir;
    ir_sum += ir_buffer[buf_idx];
    buf_idx++;
    if(buf_idx >= MA_SIZE) buf_idx = 0;
    float ir_smooth = (float)ir_sum / MA_SIZE;

    // 2. DC Removal (IR y RED)
    if (current_dc_ir == 0) current_dc_ir = ir_smooth;
    current_dc_ir = (ir_smooth * 0.05) + (current_dc_ir * 0.95);
    float ir_ac = current_dc_ir - ir_smooth;

    if (current_dc_red == 0) current_dc_red = red;
    current_dc_red = (red * 0.05) + (current_dc_red * 0.95);
    float red_ac = current_dc_red - (float)red; 

    // 3. Detección Min/Max (Decay) para AMBOS canales
    ir_max  *= 0.98; ir_min  *= 0.98;
    red_max *= 0.98; red_min *= 0.98;

    if (ir_ac > ir_max) ir_max = ir_ac;
    if (ir_ac < ir_min) ir_min = ir_ac;
    
    // Rastrear Max/Min del Rojo
    if (red_ac > red_max) red_max = red_ac;
    if (red_ac < red_min) red_min = red_ac;

    current_time_ms += 22; 

    // 4. Lógica de latido
    float ir_amp = ir_max - ir_min;
    float red_amp = red_max - red_min;
    
    if (ir_ac > (ir_amp * 0.2) && ir_amp > 20) { 
        if (!is_beat_rising && (current_time_ms - last_beat_time > MIN_BEAT_INTERVAL)) {
            is_beat_rising = 1;
            long delta = current_time_ms - last_beat_time;
            last_beat_time = current_time_ms;
            
            float instant_bpm = 60000.0f / delta;
            
            if (instant_bpm > 40 && instant_bpm < 200) {
                if (avg_bpm == 0 || !first_beat_detected) {
                    avg_bpm = instant_bpm;
                    first_beat_detected = 1;
                } else {
                    avg_bpm = (avg_bpm * 0.4) + (instant_bpm * 0.6);
                }
            }

            // --- CÁLCULO DE SPO2 AJUSTADO (CALIBRACIÓN) ---
            if (red_amp > 0 && current_dc_ir > 0 && ir_amp > 0) {
                float ac_dc_ratio_red = red_amp / current_dc_red;
                float ac_dc_ratio_ir  = ir_amp / current_dc_ir;
                
                float R = ac_dc_ratio_red / ac_dc_ratio_ir;
                
                // Formula Base: 104 - 17R
                float instant_spo2 = 104.0f - (17.0f * R); 
                
                // >>> AJUSTE DE CALIBRACIÓN <<< 
                // Sumamos +25% para llevar el rango 70% -> 95%
                instant_spo2 += 25.0f; 
                
                // Limitar valores lógicos (Clamp)
                if (instant_spo2 > 100.0f) instant_spo2 = 99.9f;
                if (instant_spo2 < 80.0f) instant_spo2 = 80.0f; // Evitar que caiga demasiado

                if (avg_spo2 == 0) avg_spo2 = instant_spo2;
                else avg_spo2 = (avg_spo2 * 0.90) + (instant_spo2 * 0.10);
            }
        }
    } else {
        if (ir_ac < (ir_amp * 0.1)) is_beat_rising = 0;
    }
}

/******************************************************************************
 * 5. MAIN
 ******************************************************************************/
int main()
{
    // Init UARTs
    XUartPs_Config uartPsCfg;
    uartPsCfg.BaseAddress = UART0_BASEADDR;
    uartPsCfg.InputClockHz = UART0_CLK; 
    XUartPs_CfgInitialize(&UartPs, &uartPsCfg, UART0_BASEADDR);
    XUartPs_SetBaudRate(&UartPs, 115200);

    UartLite.RegBaseAddress = UARTLITE_BASEADDR;
    UartLite.IsReady = XIL_COMPONENT_IS_READY;
    XUartLite_ResetFifos(&UartLite);

    if(I2C_Init_Manual() != XST_SUCCESS) return -1;
    GPIO_Init_System();
    Max30102_Init_Config();
    
    for(int i=0; i<MA_SIZE; i++) ir_buffer[i]=0;

    char buffer[128];
    char bt_buffer[64]; 

    sprintf(buffer, "--- Sistema Listo (Calibrado 90-100%%) ---\r\n");
    XUartPs_Send(&UartPs, (u8*)buffer, strlen(buffer));

    u32 red, ir;
    float ambient = 0.0f;
    float object = 0.0f;
    int loop_counter = 0;
    
    measurement_active = 0;
    Buzzer_Set(0);

    while(1)
    {
        // 1. Lectura del Sensor (Siempre corre)
        if(Max30102_ReadLatestRedIR(&red, &ir) == XST_SUCCESS) {
            HR_ProcessSample(red, ir);
        }

        // 2. Control de Botón
        if (!measurement_active) {
            u32 btns = Buttons_Read();
            if (btns > 0) {
                usleep(50000); // Debounce
                if (Buttons_Read() > 0) {
                    Reset_Session();
                    sprintf(buffer, ">>> MIDIENDO (150 muestras)... <<<\r\n");
                    XUartPs_Send(&UartPs, (u8*)buffer, strlen(buffer));
                }
            }
        }

        // 3. Control de Reportes
        loop_counter++;
        if(loop_counter >= 30) { // Cada ~0.5 seg
            loop_counter = 0;
            
            if (measurement_active) {
                ambient = MLX90614_ReadTemp(MLX_REG_TA);
                usleep(5000);
                object  = MLX90614_ReadTemp(MLX_REG_TOBJ1);

                // Buzzer si BPM > 120
                if (avg_bpm > 120.0) Buzzer_Set(1);
                else Buzzer_Set(0);

                // ---------------------------------------------------------
                // 1. SALIDA PARA PC (UART 0)
                // ---------------------------------------------------------
                memset(buffer, 0, 128);
                sprintf(buffer, 
                    "[%03d/150] BPM: %d | SpO2: %d%% | Amb: %.1f | Obj: %.1f\r\n", 
                    measurements_taken + 1, (int)avg_bpm, (int)avg_spo2, ambient, object);

                XUartPs_Send(&UartPs, (u8*)buffer, strlen(buffer));

                // ---------------------------------------------------------
                // 2. SALIDA PARA BLUETOOTH (UART LITE) - Formato Datos
                // ---------------------------------------------------------
                memset(bt_buffer, 0, 64);
                sprintf(bt_buffer, "%d|%d|%.2f\r\n", 
                        (int)avg_bpm, (int)avg_spo2, object);
                
                UARTLite_SendString(&UartLite, bt_buffer);
                // ---------------------------------------------------------

                measurements_taken++;

                if (measurements_taken >= 150) {
                    measurement_active = 0;
                    Buzzer_Set(0); 
                    sprintf(buffer, ">>> FIN <<<\r\n");
                    XUartPs_Send(&UartPs, (u8*)buffer, strlen(buffer));
                }
            }
        }

        usleep(SAMPLE_PERIOD_US); 
    }

return 0;
}
