#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/pwm.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/kfifo.h>
#include <linux/time.h>
#include <linux/pid.h>

#define DEVICE_NAME "motor_control_device"

// GPIO引脚定义
#define MOTOR1_DIR_GPIO1 4 * 32 + 11  // GPIO4_B3
#define MOTOR1_DIR_GPIO2 4 * 32 + 10  // GPIO4_B2
#define MOTOR2_DIR_GPIO1 3 * 32 + 20  // GPIO3_C4
#define MOTOR2_DIR_GPIO2 3 * 32 + 21  // GPIO3_C5
#define MOTOR1_PWM_GPIO 0 * 32 + 20  // GPIO0_C4
#define MOTOR2_PWM_GPIO 0 * 32 + 21  // GPIO0_C5
#define ALARM_GPIO 1 * 32 + 15      // GPIO1_D7

// PWM参数
#define PWM_PERIOD_NS 10000000  // PWM周期10ms
#define MAX_DUTY_CYCLE_NS 9000000  // 最大占空比90%
#define MIN_DUTY_CYCLE_NS 1000000  // 最小占空比10%



// 4G通信串口
#define MODEM_TTY "/dev/ttyUSB0"

// 物体位置结构
struct object_position {
    int x;          // 物体中心X坐标 (0-100)
    int y;          // 物体中心Y坐标 (0-100)
    int width;      // 物体宽度百分比
    bool detected;  // 是否检测到物体
};

// 电机控制结构
struct motor_control {
    struct pwm_device *pwm;
    int dir_gpio1;
    int dir_gpio2;
    int current_speed;  // 当前速度 (0-100)
    int target_speed;   // 目标速度 (0-100)
    float integral;     // PID积分项
    float prev_error;   // PID上一次误差
};

static dev_t dev_num;
struct cdev my_cdev;
int major;
int minor;

// 全局结构
static struct motor_control motor1, motor2;
static struct object_position obj_pos = {0};
static struct tty_struct *modem_tty = NULL;
static DEFINE_SPINLOCK(pos_lock);
static DECLARE_WAIT_QUEUE_HEAD(data_waitq);

struct pid_params {
    float kp;           // 比例系数
    float ki;           // 积分系数
    float kd;           // 微分系数
    float integral_max; // 积分限幅值
    float error_threshold; // 积分分离阈值
    float integral;     // 积分项
    float prev_error;   // 上一次误差
};

// 电机控制结构
struct motor_control {
    struct pwm_device *pwm;
    int dir_gpio1;
    int dir_gpio2;
    int current_speed;  // 当前速度 (0-100)
    int target_speed;   // 目标速度 (0-100)
    struct pid_params pid; // PID参数
};

// ... (其他全局定义保持不变)

// PID计算函数
static int calculate_pid(struct motor_control *motor, int target_speed)
{
    float error = target_speed - motor->current_speed;
    float derivative = error - motor->pid.prev_error;
    
    // 积分分离：仅在误差较小时使用积分项
    if (fabs(error) < motor->pid.error_threshold) {
        motor->pid.integral += error;
        
        // 积分限幅
        if (motor->pid.integral > motor->pid.integral_max) {
            motor->pid.integral = motor->pid.integral_max;
        } else if (motor->pid.integral < -motor->pid.integral_max) {
            motor->pid.integral = -motor->pid.integral_max;
        }
    } else {
        // 误差较大时重置积分项
        motor->pid.integral = 0;
    }
    
    motor->pid.prev_error = error;
    
    // 计算PID输出
    float output = motor->pid.kp * error + 
                   motor->pid.ki * motor->pid.integral + 
                   motor->pid.kd * derivative;
    
    // 限制输出范围
    if (output > 100) output = 100;
    if (output < -100) output = -100;
    
    return (int)output;
}
// 设置电机速度和方向
static void set_motor_speed(struct motor_control *motor, int speed)
{
    if (speed < 0) {
        // 反向转动
        gpio_set_value(motor->dir_gpio1, 0);
        gpio_set_value(motor->dir_gpio2, 1);
        speed = -speed;
    } else if (speed > 0) {
        // 正向转动
        gpio_set_value(motor->dir_gpio1, 1);
        gpio_set_value(motor->dir_gpio2, 0);
    } else {
        // 停止
        gpio_set_value(motor->dir_gpio1, 0);
        gpio_set_value(motor->dir_gpio2, 0);
    }
    
    // 限制速度范围
    if (speed > 100) speed = 100;
    
    // 计算PWM占空比
    u64 duty_cycle = MIN_DUTY_CYCLE_NS + 
                     (MAX_DUTY_CYCLE_NS - MIN_DUTY_CYCLE_NS) * speed / 100;
    
    // 更新PWM
    pwm_config(motor->pwm, duty_cycle, PWM_PERIOD_NS);
    motor->current_speed = speed;
}

// 4G报警函数
static void send_4g_alert(const char *message)
{
    if (!modem_tty) {
        printk(KERN_WARNING "4G modem not initialized\n");
        return;
    }
    
    struct tty_ldisc *ld;
    struct ktermios kterm;
    
    ld = tty_ldisc_ref(modem_tty);
    if (ld) {
        // 设置串口参数
        kterm = modem_tty->termios;
        cfsetospeed(&kterm, B115200);
        cfsetispeed(&kterm, B115200);
        kterm.c_cflag &= ~PARENB; // 无奇偶校验
        kterm.c_cflag &= ~CSTOPB; // 1位停止位
        kterm.c_cflag &= ~CSIZE;
        kterm.c_cflag |= CS8;    // 8位数据位
        
        tty_set_termios(modem_tty, &kterm);
        
        // 发送报警消息
        tty_write_message(modem_tty, message);
        
        tty_ldisc_deref(ld);
    }
    
    // 同时触发GPIO报警
    gpio_set_value(ALARM_GPIO, 1);
    msleep(500);
    gpio_set_value(ALARM_GPIO, 0);
}

// 基于视觉位置控制电机
static void vision_based_control(void)
{
    int obj_x, obj_detected;
    
    // 获取物体位置（受锁保护）
    spin_lock(&pos_lock);
    obj_x = obj_pos.x;
    obj_detected = obj_pos.detected;
    spin_unlock(&pos_lock);
    
    if (!obj_detected) {
        // 物体丢失 ，旋转寻找物体
        set_motor_speed(&motor1, 50);
      
        send_4g_alert("ALERT: Object lost!");
        return;
    }
    
    // 计算偏差（中心点为50）
    int error = obj_x - 50;
    
    // 根据偏差调整电机速度
    int base_speed = 60;
    int left_speed = base_speed;
    int right_speed = base_speed;
    
    if (error < -10) {
        // 物体偏左，右转
        right_speed += (-error) / 2;
    } else if (error > 10) {
        // 物体偏右，左转
        left_speed += error / 2;
    }
    
    // 使用PID计算最终速度
    motor1.target_speed = left_speed;
    motor2.target_speed = right_speed;
    
    int pid_out1 = calculate_pid(&motor1, left_speed);
    int pid_out2 = calculate_pid(&motor2, right_speed);
    
    set_motor_speed(&motor1, pid_out1);
    set_motor_speed(&motor2, pid_out2);
}

// 设备打开函数
static int device_open(struct inode *inode, struct file *file)
{
    int ret;
    
    // 申请GPIO
    ret = gpio_request(MOTOR1_DIR_GPIO1, "motor1_dir_gpio1");
    if (ret) goto error;
    gpio_direction_output(MOTOR1_DIR_GPIO1, 0);

    ret = gpio_request(MOTOR1_DIR_GPIO2, "motor1_dir_gpio2");
    if (ret) goto error;
    gpio_direction_output(MOTOR1_DIR_GPIO2, 0);

    ret = gpio_request(MOTOR2_DIR_GPIO1, "motor2_dir_gpio1");
    if (ret) goto error;
    gpio_direction_output(MOTOR2_DIR_GPIO1, 0);

    ret = gpio_request(MOTOR2_DIR_GPIO2, "motor2_dir_gpio2");
    if (ret) goto error;
    gpio_direction_output(MOTOR2_DIR_GPIO2, 0);
    
    // 报警GPIO
    ret = gpio_request(ALARM_GPIO, "alarm_gpio");
    if (ret) goto error;
    gpio_direction_output(ALARM_GPIO, 0);

    // 初始化PWM
    motor1.pwm = pwm_request(2, "MOTOR1_PWM");
    if (IS_ERR(motor1.pwm)) {
        ret = PTR_ERR(motor1.pwm);
        goto error;
    }
    
    motor2.pwm = pwm_request(4, "MOTOR2_PWM");
    if (IS_ERR(motor2.pwm)) {
        ret = PTR_ERR(motor2.pwm);
        goto error;
    }
    
    // 配置PWM
    pwm_config(motor1.pwm, 0, PWM_PERIOD_NS);
    pwm_config(motor2.pwm, 0, PWM_PERIOD_NS);
    
    // 启用PWM
    pwm_enable(motor1.pwm);
    pwm_enable(motor2.pwm);
    
    // 初始化电机控制结构
    motor1.dir_gpio1 = MOTOR1_DIR_GPIO1;
    motor1.dir_gpio2 = MOTOR1_DIR_GPIO2;
    motor1.current_speed = 0;
    motor1.target_speed = 0;
    motor1.integral = 0;
    motor1.prev_error = 0;
    
    motor2.dir_gpio1 = MOTOR2_DIR_GPIO1;
    motor2.dir_gpio2 = MOTOR2_DIR_GPIO2;
    motor2.current_speed = 0;
    motor2.target_speed = 0;
    motor2.integral = 0;
    motor2.prev_error = 0;
    
    // 初始化4G模块
    modem_tty = tty_kopen(MODEM_TTY);
    if (IS_ERR(modem_tty)) {
        printk(KERN_WARNING "Failed to open 4G modem: %s\n", MODEM_TTY);
        modem_tty = NULL;
    } else {
        printk(KERN_INFO "4G modem initialized: %s\n", MODEM_TTY);
    }
    
    printk(KERN_INFO "Motor control initialized\n");
    return 0;
    
error:
    printk(KERN_ALERT "Device open failed: %d\n", ret);
    return ret;
}

// 设备释放函数
static int device_release(struct inode *inode, struct file *file)
{
    // 停止电机
    set_motor_speed(&motor1, 0);
    set_motor_speed(&motor2, 0);
    
    // 释放PWM
    pwm_disable(motor1.pwm);
    pwm_disable(motor2.pwm);
    pwm_put(motor1.pwm);
    pwm_put(motor2.pwm);
    
    // 释放GPIO
    gpio_free(MOTOR1_DIR_GPIO1);
    gpio_free(MOTOR1_DIR_GPIO2);
    gpio_free(MOTOR2_DIR_GPIO1);
    gpio_free(MOTOR2_DIR_GPIO2);
    gpio_free(ALARM_GPIO);
    
    // 关闭4G模块
    if (modem_tty) {
        tty_kclose(modem_tty);
        modem_tty = NULL;
    }
    
    printk(KERN_INFO "Device released\n");
    return 0;
}

// 设备读取函数 - 用于获取物体位置
static ssize_t device_read(struct file *file, char __user *buffer, size_t length, loff_t *offset)
{
    struct object_position pos;
    
    if (length < sizeof(pos))
        return -EINVAL;
    
    // 复制位置数据
    spin_lock(&pos_lock);
    pos = obj_pos;
    spin_unlock(&pos_lock);
    
    if (copy_to_user(buffer, &pos, sizeof(pos)))
        return -EFAULT;
    
    return sizeof(pos);
}

// 设备写入函数 - 用于更新物体位置
static ssize_t device_write(struct file *file, const char __user *buffer, size_t length, loff_t *offset)
{
    struct object_position new_pos;
    
    if (length < sizeof(new_pos))
        return -EINVAL;
    
    if (copy_from_user(&new_pos, buffer, sizeof(new_pos)))
        return -EFAULT;
    
    // 更新位置数据
    spin_lock(&pos_lock);
    obj_pos = new_pos;
    spin_unlock(&pos_lock);
    
    // 触发控制更新
    vision_based_control();
    
    // 唤醒等待进程
    wake_up_interruptible(&data_waitq);
    
    return sizeof(new_pos);
}

// IOCTL控制函数
static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
        case 0x100: // 手动设置速度
            if (arg) {
                int speed = (int)arg;
                set_motor_speed(&motor1, speed);
                set_motor_speed(&motor2, speed);
            }
            break;
            
        case 0x200: // 触发报警
            send_4g_alert("ALERT: Manual trigger!");
            break;
            
        default:
            return -ENOTTY;
    }
    
    return 0;
}

// Poll函数 - 等待位置更新
static unsigned int device_poll(struct file *file, poll_table *wait)
{
    unsigned int mask = 0;
    
    poll_wait(file, &data_waitq, wait);
    
    spin_lock(&pos_lock);
    if (obj_pos.detected)
        mask |= POLLIN | POLLRDNORM;
    spin_unlock(&pos_lock);
    
    return mask;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = device_open,
    .release = device_release,
    .read = device_read,
    .write = device_write,
    .unlocked_ioctl = device_ioctl,
    .poll = device_poll,
};

static int __init mydevice_init(void)
{
    int ret;
    
    // 注册字符设备
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0)
        return ret;
    
    major = MAJOR(dev_num);
    minor = MINOR(dev_num);
    
    cdev_init(&my_cdev, &fops);
    my_cdev.owner = THIS_MODULE;
    
    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret < 0) {
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }
    
    printk(KERN_INFO "Motor control device registered (major %d)\n", major);
    return 0;
}

static void __exit mydevice_exit(void)
{
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "Motor control device unregistered\n");
}

module_init(mydevice_init);
module_exit(mydevice_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded System Engineer");
MODULE_DESCRIPTION("Advanced Motor Control with PID, Vision Tracking and 4G Alert");
