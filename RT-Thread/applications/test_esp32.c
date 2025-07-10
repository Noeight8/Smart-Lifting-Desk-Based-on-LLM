#include <rtthread.h>
#include <rtdevice.h>

static rt_device_t uart_dev;
static struct rt_semaphore uart_rx_sem;

// 接收回调
static rt_err_t uart_rx_callback(rt_device_t dev, rt_size_t size) {
    if (size > 0) rt_sem_release(&uart_rx_sem);
    return RT_EOK;
}

// 初始化UART（纯RT-Thread API）
void uart_init(void) {
    // 1. 查找设备
    uart_dev = rt_device_find("uart5");
    if (!uart_dev) {
        rt_kprintf("[ERROR] UART5 not found!\n");
        return;
    }

    // 2. 初始化信号量（确保线程安全）
    static rt_bool_t sem_inited = RT_FALSE;
    if (!sem_inited) {
        rt_sem_init(&uart_rx_sem, "uart_rx", 0, RT_IPC_FLAG_FIFO);
        sem_inited = RT_TRUE;
    }

    // 3. 配置波特率（通过RT-Thread控制）
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;
    config.baud_rate = BAUD_RATE_115200;
    if (rt_device_control(uart_dev, RT_DEVICE_CTRL_CONFIG, &config) != RT_EOK) {
        rt_kprintf("[ERROR] Config UART5 failed!\n");
        return;
    }

    // 4. 设置回调并打开设备
    rt_device_set_rx_indicate(uart_dev, uart_rx_callback);
    if (rt_device_open(uart_dev, RT_DEVICE_FLAG_INT_RX) != RT_EOK) {
        rt_kprintf("[ERROR] Open UART5 failed!\n");
        return;
    }
    rt_kprintf("[OK] UART5 initialized.\n");
}

// 测试线程
static void uart_test_thread(void *param) {
    char ch;
    while (1) {
        if (rt_sem_take(&uart_rx_sem, RT_WAITING_FOREVER) == RT_EOK) {
            while (rt_device_read(uart_dev, 0, &ch, 1) == 1) {
                rt_device_write(uart_dev, 0, &ch, 1); // 回显
                rt_kprintf("Received: %c\n", ch);
            }
        }
    }
}

// 启动线程
void uart_test_start(void) {
    static rt_thread_t tid = RT_NULL;
    if (!tid) {
        tid = rt_thread_create("uart_test", uart_test_thread, RT_NULL, 1024, 20, 10);
        if (tid) rt_thread_startup(tid);
    }
}
void uart_send_test(void) {
    char msg[] = "Hello UART5!\n";
    rt_device_write(uart_dev, 0, msg, sizeof(msg));
}
MSH_CMD_EXPORT(uart_send_test, Send test message);
MSH_CMD_EXPORT(uart_init, Initialize UART5);
MSH_CMD_EXPORT(uart_test_start, Start UART echo test);
