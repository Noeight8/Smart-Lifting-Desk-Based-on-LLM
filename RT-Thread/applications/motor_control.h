/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-07-07     YZL18       the first version
 */
#ifndef APPLICATIONS_MOTOR_CONTROL_H_
#define APPLICATIONS_MOTOR_CONTROL_H_
#ifndef __MOTOR_CONTROL_H__
#define __MOTOR_CONTROL_H__
#define UART_DEV_NAME "uart5"  // 假设使用UART5与ESP32通信
#include <rtthread.h>
#include <rtdevice.h>
#include <drv_gpio.h>
#include <drv_pwm.h>

// 电机引脚定义
#define MOTOR1_IN1_PIN    GET_PIN(10, 2)  // P8_0
#define MOTOR1_IN2_PIN    GET_PIN(10, 3)  // P8_1
#define MOTOR2_IN1_PIN    GET_PIN(10, 4)  // P8_2
#define MOTOR2_IN2_PIN    GET_PIN(10, 5)  // P8_3

// PWM通道定义
#define MOTOR1_PWM_DEV    "pwm0"
#define MOTOR1_PWM_CH     0              // P9_0
#define MOTOR2_PWM_DEV    "pwm0"
#define MOTOR2_PWM_CH     1              // P9_1

// 电机状态枚举
typedef enum {
    MOTOR_STOP = 0,
    MOTOR_FORWARD,
    MOTOR_BACKWARD
} motor_state_t;

// 电机控制结构体
typedef struct {
    motor_state_t state;
    rt_int8_t speed;  // 0-100
} motor_control_t;

// 函数声明
void motor_init(void);
void motor_set_speed(uint8_t motor_num, rt_int8_t speed);
void motor_set_direction(uint8_t motor_num, motor_state_t direction);
void motor_stop(uint8_t motor_num);
void motor_control_test(void);

// 为语音控制预留的接口
void voice_control_init(void);  // 语音控制初始化
void process_voice_command(const char *cmd);  // 处理语音命令

#endif /* __MOTOR_CONTROL_H__ */


#endif /* APPLICATIONS_MOTOR_CONTROL_H_ */
