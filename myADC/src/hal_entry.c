#include "hal_data.h"
#include "common_utils.h"
#include "rm_comms_i2c.h"
#include "rm_zmod4xxx.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/*
 * NOTE:
 * - This version targets ZMOD4410 IAQ 2nd Generation
 * - It keeps UART output to ESP8266
 * - It uses polling (no IRQ driver required)
 * - If your board uses a different RES_N pin, change ZMOD4410_RES_N_PIN
 */
#define ZMOD4410_RES_N_PIN    BSP_IO_PORT_04_PIN_07
/*--------------------------*/
// Các con số W và Bias bạn vừa tìm được từ file train.py
#define W1_IAQ_CURRENT  0.999093f // Thay số của bạn vào
#define W2_IAQ_DIFF     -0.459130f // Thay số của bạn vào
#define W3_TVOC_DIFF    4.582203f // Thay số của bạn vào
#define B_BIAS          0.002128f // Thay số của bạn vào
/*------------------------------*/
/* UART TX state */
volatile bool g_uart_tx_done = false;
volatile bool g_uart_tx_busy = false;

/* ZMOD4XXX callback states */
volatile bool g_zmod4xxx_i2c_done = false;
volatile bool g_zmod4xxx_irq_done = false;
volatile rm_zmod4xxx_event_t g_zmod4xxx_i2c_event = RM_ZMOD4XXX_EVENT_SUCCESS;
volatile rm_zmod4xxx_event_t g_zmod4xxx_irq_event = RM_ZMOD4XXX_EVENT_SUCCESS;

/* Print buffer */
static char g_print_buf[128] = {0};

/* ZMOD raw + calculated data */
static rm_zmod4xxx_raw_data_t      g_zmod_raw;
static rm_zmod4xxx_iaq_2nd_data_t  g_zmod_data;

/*---------------------------------*/
/* Biến lưu giá trị cũ để tính độ lệch */
static float g_last_iaq = 0.0f;
static float g_last_tvoc = 0.0f;
static bool g_first_run = true; // Để tránh nhảy số ảo lần đầu tiên
/* ---------------- UART callback ---------------- */
void user_uart_callback(uart_callback_args_t * p_args)
{
    if (NULL == p_args)
    {
        return;
    }

    if (UART_EVENT_TX_COMPLETE == p_args->event)
    {
        g_uart_tx_done = true;
        g_uart_tx_busy = false;
    }
}

/* ---------------- ZMOD callbacks ---------------- */
void zmod4xxx_comms_i2c_callback(rm_zmod4xxx_callback_args_t * p_args)
{
    if (NULL == p_args)
    {
        return;
    }

    g_zmod4xxx_i2c_event = p_args->event;
    g_zmod4xxx_i2c_done  = true;
}

void zmod4xxx_irq_callback(rm_zmod4xxx_callback_args_t * p_args)
{
    if (NULL == p_args)
    {
        return;
    }

    g_zmod4xxx_irq_event = p_args->event;
    g_zmod4xxx_irq_done  = true;
}

/* ---------------- UART helper ---------------- */
static fsp_err_t uart_send_string(char * p_data)
{
    fsp_err_t err;
    uint32_t len = (uint32_t) strlen(p_data);

    if (0U == len)
    {
        return FSP_SUCCESS;
    }

    while (g_uart_tx_busy)
    {
    }

    g_uart_tx_done = false;
    g_uart_tx_busy = true;

    err = R_SCI_UART_Write(&g_uart0_ctrl, (uint8_t *) p_data, len);
    if (FSP_SUCCESS != err)
    {
        g_uart_tx_busy = false;
        return err;
    }

    while (!g_uart_tx_done)
    {
    }

    return FSP_SUCCESS;
}

/* ---------------- I2C bus open helper ---------------- */
static fsp_err_t zmod4xxx_bus_open(void)
{
    rm_comms_i2c_bus_extended_cfg_t * p_extend =
        (rm_comms_i2c_bus_extended_cfg_t *) g_zmod4xxx_sensor0_cfg.p_comms_instance->p_cfg->p_extend;

    i2c_master_instance_t * p_driver_instance =
        (i2c_master_instance_t *) p_extend->p_driver_instance;

    fsp_err_t err = p_driver_instance->p_api->open(p_driver_instance->p_ctrl,
                                                   p_driver_instance->p_cfg);

    if ((FSP_SUCCESS == err) || (FSP_ERR_ALREADY_OPEN == err))
    {
        return FSP_SUCCESS;
    }

    return err;
}

/* ---------------- Hardware reset for ZMOD4410 ---------------- */
static void zmod4410_reset(void)
{
    /* FSP sample resets RES_N high -> low -> high with delays */
    R_IOPORT_PinWrite(&g_ioport_ctrl, ZMOD4410_RES_N_PIN, BSP_IO_LEVEL_HIGH);
    R_BSP_SoftwareDelay(10U, BSP_DELAY_UNITS_MILLISECONDS);

    R_IOPORT_PinWrite(&g_ioport_ctrl, ZMOD4410_RES_N_PIN, BSP_IO_LEVEL_LOW);
    R_BSP_SoftwareDelay(10U, BSP_DELAY_UNITS_MILLISECONDS);

    R_IOPORT_PinWrite(&g_ioport_ctrl, ZMOD4410_RES_N_PIN, BSP_IO_LEVEL_HIGH);
    R_BSP_SoftwareDelay(10U, BSP_DELAY_UNITS_MILLISECONDS);
}

/* ---------------- Wait helpers ---------------- */
static void zmod_wait_i2c_done(void)
{
    while (!g_zmod4xxx_i2c_done)
    {
    }
    g_zmod4xxx_i2c_done = false;
}

/* ---------------- Read IAQ 2nd Gen data once ---------------- */
static fsp_err_t zmod4410_read_once(float * p_iaq, float * p_tvoc, float * p_eco2)
{
    fsp_err_t err;

    /* Start measurement */
    g_zmod4xxx_i2c_done = false;
    err = RM_ZMOD4XXX_MeasurementStart(&g_zmod4xxx_sensor0_ctrl);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    zmod_wait_i2c_done();

    /* Required delay for IAQ 2nd Gen measurement */
    R_BSP_SoftwareDelay(3000U, BSP_DELAY_UNITS_MILLISECONDS);

    /* Poll until measurement is complete */
    do
    {
        g_zmod4xxx_i2c_done = false;
        err = RM_ZMOD4XXX_StatusCheck(&g_zmod4xxx_sensor0_ctrl);
        if (FSP_SUCCESS != err)
        {
            return err;
        }

        zmod_wait_i2c_done();

        g_zmod4xxx_i2c_done = false;
        err = RM_ZMOD4XXX_DeviceErrorCheck(&g_zmod4xxx_sensor0_ctrl);
        if (FSP_SUCCESS != err)
        {
            return err;
        }

        zmod_wait_i2c_done();

        if ((RM_ZMOD4XXX_EVENT_DEV_ERR_POWER_ON_RESET == g_zmod4xxx_i2c_event) ||
            (RM_ZMOD4XXX_EVENT_DEV_ERR_ACCESS_CONFLICT == g_zmod4xxx_i2c_event))
        {
            return FSP_ERR_ABORTED;
        }

        err = RM_ZMOD4XXX_Read(&g_zmod4xxx_sensor0_ctrl, &g_zmod_raw);
        if (FSP_ERR_SENSOR_MEASUREMENT_NOT_FINISHED == err)
        {
            R_BSP_SoftwareDelay(50U, BSP_DELAY_UNITS_MILLISECONDS);
        }

    } while (FSP_ERR_SENSOR_MEASUREMENT_NOT_FINISHED == err);

    if (FSP_SUCCESS != err)
    {
        return err;
    }

    g_zmod4xxx_i2c_done = false;
    zmod_wait_i2c_done();

    /* Device error check again after read */
    g_zmod4xxx_i2c_done = false;
    err = RM_ZMOD4XXX_DeviceErrorCheck(&g_zmod4xxx_sensor0_ctrl);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    zmod_wait_i2c_done();

    if ((RM_ZMOD4XXX_EVENT_DEV_ERR_POWER_ON_RESET == g_zmod4xxx_i2c_event) ||
        (RM_ZMOD4XXX_EVENT_DEV_ERR_ACCESS_CONFLICT == g_zmod4xxx_i2c_event))
    {
        return FSP_ERR_ABORTED;
    }

    /*
     * IAQ 2nd Gen example in FSP sets ambient temperature/humidity before DataCalculate.
     * If this causes issues in your build/runtime, comment out these 3 lines first.
     */
    err = RM_ZMOD4XXX_TemperatureAndHumiditySet(&g_zmod4xxx_sensor0_ctrl, 20.0f, 50.0f);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    err = RM_ZMOD4XXX_Iaq2ndGenDataCalculate(&g_zmod4xxx_sensor0_ctrl, &g_zmod_raw, &g_zmod_data);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    *p_iaq  = g_zmod_data.iaq;
    *p_tvoc = g_zmod_data.tvoc;
    *p_eco2 = g_zmod_data.eco2;

    return FSP_SUCCESS;
}

/*-----------------------------------------------*/

float calculate_ai_prediction(float current_iaq, float current_tvoc)
{
    float iaq_diff = 0.0f;
    float tvoc_diff = 0.0f;

    if (g_first_run)
    {
        g_first_run = false;
        g_last_iaq = current_iaq;
        g_last_tvoc = current_tvoc;
    }

    // Tính toán độ lệch (Tiền xử lý dữ liệu)
    iaq_diff = current_iaq - g_last_iaq;
    tvoc_diff = current_tvoc - g_last_tvoc;

    // Cập nhật giá trị cũ cho lần sau
    g_last_iaq = current_iaq;
    g_last_tvoc = current_tvoc;

    // Công thức Edge AI chạy trực tiếp trên Node
    float prediction = (W1_IAQ_CURRENT * current_iaq) +
                       (W2_IAQ_DIFF * iaq_diff) +
                       (W3_TVOC_DIFF * tvoc_diff) + B_BIAS;

    return prediction;
}
/* ---------------- Main entry ---------------- */
void hal_entry(void)
{
    fsp_err_t err;
    float iaq  = 0.0f;
    float tvoc = 0.0f;
    float eco2 = 0.0f;

    /* Open UART */
    err = R_SCI_UART_Open(&g_uart0_ctrl, &g_uart0_cfg);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("R_SCI_UART_Open failed\r\n");
        __BKPT(1);
    }

    /* Open I2C bus */
    err = zmod4xxx_bus_open();
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("ZMOD I2C bus open failed\r\n");
        __BKPT(1);
    }

    /* Reset sensor */
    zmod4410_reset();

    /* Open ZMOD4410 */
    err = RM_ZMOD4XXX_Open(&g_zmod4xxx_sensor0_ctrl, &g_zmod4xxx_sensor0_cfg);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("RM_ZMOD4XXX_Open failed\r\n");
        __BKPT(1);
    }

    while (1)
    {
        err = zmod4410_read_once(&iaq, &tvoc, &eco2);

        if (FSP_SUCCESS == err)
        {
            float iaq_pred = calculate_ai_prediction(iaq, tvoc);
            snprintf(g_print_buf, sizeof(g_print_buf),
                             "IAQ:%.2f,TVOC:%.2f,ECO2:%.2f,PRED:%.2f\r\n",
                             iaq, tvoc, eco2, iaq_pred);

            err = uart_send_string(g_print_buf);
            if (FSP_SUCCESS != err)
            {
                APP_ERR_PRINT("UART send failed\r\n");
                __BKPT(1);
            }
        }
        else if ((FSP_ERR_SENSOR_IN_STABILIZATION == err) ||
                 (FSP_ERR_SENSOR_INVALID_DATA == err))
        {
            snprintf(g_print_buf, sizeof(g_print_buf),
                     "STATE:WARMUP\r\n");
            uart_send_string(g_print_buf);

            APP_PRINT("ZMOD4410 warming up / invalid data\r\n");
        }
        else
        {
            snprintf(g_print_buf, sizeof(g_print_buf),
                     "STATE:ERR,%d\r\n",
                     (int) err);
            uart_send_string(g_print_buf);

            APP_ERR_PRINT("ZMOD4410 read failed\r\n");
        }

        R_BSP_SoftwareDelay(1000U, BSP_DELAY_UNITS_MILLISECONDS);
    }
}

#if BSP_TZ_SECURE_BUILD
BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable(void);

BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable(void)
{
}
#endif
