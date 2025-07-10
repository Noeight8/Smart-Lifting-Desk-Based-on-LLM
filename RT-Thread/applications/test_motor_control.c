#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>
#include <drv_gpio.h>  // 必须包含此头文件

/******************** 硬件引脚定义 ********************/
#define MOTOR1_IN1  GET_PIN(10, 0)  // P8_0
#define MOTOR1_IN2  GET_PIN(10, 1)  // P8_1
#define MOTOR2_IN1  GET_PIN(10, 2)  // P8_2
#define MOTOR2_IN2  GET_PIN(10, 3)  // P8_3

/******************** 电机状态枚举 ********************/
typedef enum {
    MOTOR_STOP = 0,
    MOTOR_FORWARD,
    MOTOR_BACKWARD
} motor_state_t;

/******************** 电机控制函数 ********************/
static void motor_init(void)
{
    // 初始化GPIO
    rt_pin_mode(MOTOR1_IN1, PIN_MODE_OUTPUT);
    rt_pin_mode(MOTOR1_IN2, PIN_MODE_OUTPUT);
    rt_pin_mode(MOTOR2_IN1, PIN_MODE_OUTPUT);
    rt_pin_mode(MOTOR2_IN2, PIN_MODE_OUTPUT);

    // 默认停止电机
    rt_pin_write(MOTOR1_IN1, PIN_LOW);
    rt_pin_write(MOTOR1_IN2, PIN_LOW);
    rt_pin_write(MOTOR2_IN1, PIN_LOW);
    rt_pin_write(MOTOR2_IN2, PIN_LOW);

    rt_kprintf("[Motor] Pins initialized (ENA/ENB=HIGH)\n");
}

static void motor_control(uint8_t motor_num, motor_state_t state)
{
    switch(motor_num) {
        case 1: // 电机1控制
            rt_pin_write(MOTOR1_IN1, (state == MOTOR_FORWARD) ? PIN_HIGH : PIN_LOW);
            rt_pin_write(MOTOR1_IN2, (state == MOTOR_BACKWARD) ? PIN_HIGH : PIN_LOW);
            rt_kprintf("Motor1: %s\n",
                      state == MOTOR_FORWARD ? "FORWARD" :
                      state == MOTOR_BACKWARD ? "BACKWARD" : "STOP");
            break;

        case 2: // 电机2控制
            rt_pin_write(MOTOR2_IN1, (state == MOTOR_FORWARD) ? PIN_HIGH : PIN_LOW);
            rt_pin_write(MOTOR2_IN2, (state == MOTOR_BACKWARD) ? PIN_HIGH : PIN_LOW);
            rt_kprintf("Motor2: %s\n",
                      state == MOTOR_FORWARD ? "FORWARD" :
                      state == MOTOR_BACKWARD ? "BACKWARD" : "STOP");
            break;
    }
}

/******************** CLI命令处理 ********************/
static void motor(int argc, char **argv)
{
    if (argc < 3) {
        rt_kprintf("Usage: motor_test [1|2|all] [fwd|rev|stop]\n");
        rt_kprintf("Example:\n");
        rt_kprintf("  motor_test 1 fwd    # Motor1 forward\n");
        rt_kprintf("  motor_test all stop # Stop all motors\n");
        return;
    }

    // 解析电机编号
    uint8_t motor_num = 0; // 0=all, 1=motor1, 2=motor2
    if (strcmp(argv[1], "1") == 0) motor_num = 1;
    else if (strcmp(argv[1], "2") == 0) motor_num = 2;
    else if (strcmp(argv[1], "all") != 0) {
        rt_kprintf("Error: Invalid motor number!\n");
        return;
    }

    // 解析指令
    motor_state_t state;
    if (strcmp(argv[2], "fwd") == 0) state = MOTOR_FORWARD;
    else if (strcmp(argv[2], "rev") == 0) state = MOTOR_BACKWARD;
    else if (strcmp(argv[2], "stop") == 0) state = MOTOR_STOP;
    else {
        rt_kprintf("Error: Invalid command!\n");
        return;
    }

    // 执行控制
    if (motor_num == 0) {
        motor_control(1, state);
        motor_control(2, state);
    } else {
        motor_control(motor_num, state);
    }
}

/******************** 初始化 ********************/
static int motor_cli_init(void)
{
    motor_init();
    return 0;
}
INIT_APP_EXPORT(motor_cli_init);  // 自动初始化
MSH_CMD_EXPORT(motor, Control motors: motor [1|2|all] [fwd|rev|stop]);
