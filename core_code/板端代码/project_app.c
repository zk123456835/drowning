#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <signal.h>
#include <opencv2/opencv.hpp>
#include <json-c/json.h>  // 用于配置加载

#define DEV_NAME "/dev/motor_control_device"
#define CONFIG_FILE "motor_config.json"

// 物体位置结构（与驱动一致）
struct object_position {
    int x;
    int y;
    int width;
    bool detected;
};

// 配置结构
struct app_config {
    int camera_index;
    int hsv_low[3];
    int hsv_high[3];
    int min_object_size;
    int max_object_size;
    int base_speed;
    int control_gain;
    int frame_width;
    int frame_height;
    bool debug_mode;
};

// 全局变量
volatile sig_atomic_t stop = 0;
int dev_fd = -1;
cv::VideoCapture cap;
struct app_config config;

// 信号处理函数
void sigint_handler(int signum) {
    stop = 1;
    printf("\nReceived SIGINT, shutting down...\n");
}

// 加载配置文件
int load_config(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Failed to open config file");
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *data = (char *)malloc(len + 1);
    fread(data, 1, len, fp);
    fclose(fp);
    data[len] = '\0';

    json_object *root = json_tokener_parse(data);
    if (!root) {
        fprintf(stderr, "Failed to parse config file\n");
        free(data);
        return -1;
    }

    // 解析配置
    config.camera_index = json_object_get_int(json_object_object_get(root, "camera_index"));
    
    json_object *hsv_low = json_object_object_get(root, "hsv_low");
    config.hsv_low[0] = json_object_get_int(json_object_array_get_idx(hsv_low, 0));
    config.hsv_low[1] = json_object_get_int(json_object_array_get_idx(hsv_low, 1));
    config.hsv_low[2] = json_object_get_int(json_object_array_get_idx(hsv_low, 2));
    
    json_object *hsv_high = json_object_object_get(root, "hsv_high");
    config.hsv_high[0] = json_object_get_int(json_object_array_get_idx(hsv_high, 0));
    config.hsv_high[1] = json_object_get_int(json_object_array_get_idx(hsv_high, 1));
    config.hsv_high[2] = json_object_get_int(json_object_array_get_idx(hsv_high, 2));
    
    config.min_object_size = json_object_get_int(json_object_object_get(root, "min_object_size"));
    config.max_object_size = json_object_get_int(json_object_object_get(root, "max_object_size"));
    config.base_speed = json_object_get_int(json_object_object_get(root, "base_speed"));
    config.control_gain = json_object_get_int(json_object_object_get(root, "control_gain"));
    config.frame_width = json_object_get_int(json_object_object_get(root, "frame_width"));
    config.frame_height = json_object_get_int(json_object_object_get(root, "frame_height"));
    config.debug_mode = json_object_get_boolean(json_object_object_get(root, "debug_mode"));
    
    json_object_put(root);
    free(data);
    
    return 0;
}

// 视觉处理函数
struct object_position detect_object(cv::Mat &frame) {
    struct object_position pos = {0};
    
    // 调整大小
    cv::resize(frame, frame, cv::Size(config.frame_width, config.frame_height));
    
    // 转换为HSV颜色空间
    cv::Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
    
    // 创建颜色掩码
    cv::Scalar lower(config.hsv_low[0], config.hsv_low[1], config.hsv_low[2]);
    cv::Scalar upper(config.hsv_high[0], config.hsv_high[1], config.hsv_high[2]);
    cv::Mat mask;
    cv::inRange(hsv, lower, upper, mask);
    
    // 形态学操作
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);
    
    // 查找轮廓
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    if (!contours.empty()) {
        // 找到最大轮廓
        int max_idx = -1;
        double max_area = 0;
        
        for (size_t i = 0; i < contours.size(); i++) {
            double area = cv::contourArea(contours[i]);
            if (area > max_area) {
                max_area = area;
                max_idx = i;
            }
        }
        
        if (max_idx >= 0 && max_area > config.min_object_size && max_area < config.max_object_size) {
            // 计算边界框
            cv::Rect bbox = cv::boundingRect(contours[max_idx]);
            
            // 计算中心位置 (百分比)
            pos.x = (bbox.x + bbox.width/2) * 100 / frame.cols;
            pos.y = (bbox.y + bbox.height/2) * 100 / frame.rows;
            pos.width = bbox.width * 100 / frame.cols;
            pos.detected = true;
            
            // 在图像上绘制
            if (config.debug_mode) {
                cv::rectangle(frame, bbox, cv::Scalar(0, 255, 0), 2);
                cv::circle(frame, cv::Point(bbox.x+bbox.width/2, bbox.y+bbox.height/2), 
                          5, cv::Scalar(0, 0, 255), -1);
                cv::putText(frame, "Tracking", cv::Point(10, 30), 
                          cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 0), 2);
            }
        }
    }
    
    if (!pos.detected && config.debug_mode) {
        cv::putText(frame, "Searching...", cv::Point(10, 30), 
                  cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2);
    }
    
    return pos;
}

// 初始化设备
int init_devices() {
    // 打开设备
    dev_fd = open(DEV_NAME, O_RDWR);
    if (dev_fd < 0) {
        perror("Open device failed");
        return -1;
    }
    
    // 打开摄像头
    cap.open(config.camera_index);
    if (!cap.isOpened()) {
        fprintf(stderr, "Failed to open camera %d\n", config.camera_index);
        close(dev_fd);
        return -1;
    }
    
    // 设置摄像头参数
    cap.set(cv::CAP_PROP_FRAME_WIDTH, config.frame_width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, config.frame_height);
    cap.set(cv::CAP_PROP_FPS, 30);
    
    printf("Devices initialized:\n");
    printf("  - Motor control device: %s\n", DEV_NAME);
    printf("  - Camera: index %d, resolution %dx%d\n", 
           config.camera_index, config.frame_width, config.frame_height);
    
    return 0;
}

// 关闭设备
void cleanup_devices() {
    if (dev_fd >= 0) {
        // 发送停止命令
        struct object_position stop_cmd = {0};
        stop_cmd.detected = false;
        write(dev_fd, &stop_cmd, sizeof(stop_cmd));
        
        close(dev_fd);
        printf("Motor device closed\n");
    }
    
    if (cap.isOpened()) {
        cap.release();
        printf("Camera released\n");
    }
    
    if (config.debug_mode) {
        cv::destroyAllWindows();
    }
}

// 主控制循环
void run_control_loop() {
    cv::Mat frame;
    struct pollfd pfd = {
        .fd = dev_fd,
        .events = POLLIN
    };
    
    int frame_count = 0;
    time_t start_time = time(NULL);
    int lost_count = 0;
    const int max_lost_frames = 30; // 30帧后报警
    
    printf("Starting control loop...\n");
    
    while (!stop) {
        // 捕获视频帧
        if (!cap.read(frame)) {
            fprintf(stderr, "Failed to capture frame\n");
            usleep(100000); // 100ms
            continue;
        }
        
        // 检测物体
        struct object_position pos = detect_object(frame);
        
        if (!pos.detected) {
            lost_count++;
            if (lost_count > max_lost_frames) {
                // 连续多帧未检测到物体，触发报警
                ioctl(dev_fd, 0x200); // 触发报警
                lost_count = 0;
            }
        } else {
            lost_count = 0;
        }
        
        // 发送位置信息到驱动
        if (write(dev_fd, &pos, sizeof(pos)) != sizeof(pos)) {
            perror("Write to device failed");
            break;
        }
        
        // 检查是否有报警信息（非阻塞）
        if (poll(&pfd, 1, 0) > 0) {
            if (pfd.revents & POLLIN) {
                struct object_position driver_pos;
                if (read(dev_fd, &driver_pos, sizeof(driver_pos)) == sizeof(driver_pos)) {
                    if (!driver_pos.detected) {
                        printf("ALERT: Object lost detected in driver!\n");
                    }
                }
            }
        }
        
        // 显示结果
        if (config.debug_mode) {
            cv::imshow("Object Tracking", frame);
            if (cv::waitKey(1) == 27) { // ESC退出
                stop = 1;
            }
        }
        
        // 性能统计
        frame_count++;
        time_t current_time = time(NULL);
        if (current_time - start_time >= 5) {
            printf("FPS: %.2f\n", frame_count / 5.0);
            frame_count = 0;
            start_time = current_time;
        }
    }
}

int main(int argc, char *argv[]) {
    // 注册信号处理
    signal(SIGINT, sigint_handler);
    
    printf("Motor Control Application\n");
    printf("------------------------\n");
    
    // 加载配置
    if (load_config(CONFIG_FILE) != 0) {
        fprintf(stderr, "Using default configuration\n");
        // 默认配置
        config.camera_index = 0;
        config.hsv_low[0] = 0; config.hsv_low[1] = 70; config.hsv_low[2] = 50;
        config.hsv_high[0] = 10; config.hsv_high[1] = 255; config.hsv_high[2] = 255;
        config.min_object_size = 500;
        config.max_object_size = 30000;
        config.base_speed = 60;
        config.control_gain = 30;
        config.frame_width = 640;
        config.frame_height = 480;
        config.debug_mode = true;
    }
    
    // 打印配置
    printf("Configuration:\n");
    printf("  HSV Range: [%d,%d,%d] to [%d,%d,%d]\n",
           config.hsv_low[0], config.hsv_low[1], config.hsv_low[2],
           config.hsv_high[0], config.hsv_high[1], config.hsv_high[2]);
    printf("  Object size: %d-%d pixels\n", config.min_object_size, config.max_object_size);
    printf("  Base speed: %d%%, Control gain: %d\n", config.base_speed, config.control_gain);
    printf("  Resolution: %dx%d\n", config.frame_width, config.frame_height);
    printf("  Debug mode: %s\n", config.debug_mode ? "enabled" : "disabled");
    
    // 初始化设备
    if (init_devices() != 0) {
        fprintf(stderr, "Device initialization failed\n");
        return 1;
    }
    
    // 运行主控制循环
    run_control_loop();
    
    // 清理资源
    cleanup_devices();
    
    printf("Application exited gracefully.\n");
    return 0;
}
