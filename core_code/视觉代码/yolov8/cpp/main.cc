// Copyright (c) 2023 by Rockchip Electronics Co., Ltd. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*-------------------------------------------
                Includes
-------------------------------------------*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yolov8.h"
#include "image_utils.h"
#include "file_utils.h"
#include "image_drawing.h"
#include <sys/time.h>
#include <opencv2/opencv.hpp> // 添加 OpenCV 库
#if defined(RV1106_1103) 
    #include "dma_alloc.hpp"
#endif

/*

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("Usage: %s <model_path> <camera_id>\n", argv[0]);
        printf("Example: %s model/yolov8.rknn 0\n", argv[0]);
        return -1;
    }

    const char *model_path = argv[1];
    int camera_id = atoi(argv[2]); // 摄像头设备号（如 0 表示 /dev/video0）

    // 初始化摄像头
    cv::VideoCapture cap(camera_id);
    if (!cap.isOpened())
    {
        printf("Error: Could not open camera %d\n", camera_id);
        return -1;
    }

    // 设置摄像头分辨率（根据模型输入调整）
    int model_width = 640;  // 与模型输入一致
    int model_height = 640;
    cap.set(cv::CAP_PROP_FRAME_WIDTH, model_width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, model_height);

    // 初始化模型
    rknn_app_context_t rknn_app_ctx;
    memset(&rknn_app_ctx, 0, sizeof(rknn_app_context_t));
    int ret = init_yolov8_model(model_path, &rknn_app_ctx);
    if (ret != 0)
    {
        printf("init_yolov8_model fail! ret=%d\n", ret);
        return -1;
    }

    init_post_process(); // 初始化标签

    cv::Mat frame;
    cv::namedWindow("YOLOv8 Camera Detection", cv::WINDOW_NORMAL);

    // 帧率计算
    struct timeval start_time, end_time;
    float fps = 0;
    int frame_count = 0;

    while (true)
    {
        gettimeofday(&start_time, NULL);

        // 读取摄像头帧
        if (!cap.read(frame))
        {
            printf("Error: Failed to read frame\n");
            break;
        }

        // 转换图像格式（BGR -> RGB）
        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);

        // 准备输入图像缓冲区
        image_buffer_t src_image;
        memset(&src_image, 0, sizeof(image_buffer_t));
        src_image.virt_addr = frame.data; // 直接使用 OpenCV 内存
        src_image.width = frame.cols;
        src_image.height = frame.rows;
        src_image.format = IMAGE_FORMAT_RGB888;

        // 执行推理
        object_detect_result_list od_results;
        ret = inference_yolov8_model(&rknn_app_ctx, &src_image, &od_results);
        if (ret != 0)
        {
            printf("inference_yolov8_model fail! ret=%d\n", ret);
            continue;
        }

        // 绘制检测结果
        for (int i = 0; i < od_results.count; i++)
        {
            object_detect_result *det_result = &(od_results.results[i]);
            int x1 = det_result->box.left;
            int y1 = det_result->box.top;
            int x2 = det_result->box.right;
            int y2 = det_result->box.bottom;

            // 绘制框和标签
            cv::rectangle(frame, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(0, 0, 255), 2);
            std::string label = cv::format("%s %.1f%%", coco_cls_to_name(det_result->cls_id), det_result->prop * 100);
            cv::putText(frame, label, cv::Point(x1, y1 - 10), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
        }

        // 计算并显示帧率
        gettimeofday(&end_time, NULL);
        float time_use = (end_time.tv_sec - start_time.tv_sec) * 1000 + (end_time.tv_usec - start_time.tv_usec) / 1000.0;
        fps = 1000.0 / time_use;
        cv::putText(frame, cv::format("FPS: %.2f", fps), cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 0, 0), 2);

        // 显示结果
        cv::imshow("YOLOv8 Camera Detection", frame);

        // 按 ESC 退出
        if (cv::waitKey(1) == 27)
        {
            break;
        }
    }

    // 释放资源
    cap.release();
    cv::destroyAllWindows();
    release_yolov8_model(&rknn_app_ctx);
    deinit_post_process();

    return 0;
}
*/
int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("Usage: %s <model_path> <camera_id>\n", argv[0]);
        printf("Example: %s model/yolov8.rknn 0\n", argv[0]);
        return -1;
    }

    const char *model_path = argv[1];
    int camera_id = atoi(argv[2]);

    // 初始化摄像头
    cv::VideoCapture cap(camera_id);
    if (!cap.isOpened())
    {
        printf("Error: Could not open camera %d\n", camera_id);
        return -1;
    }

    // 设置摄像头分辨率
    int model_width = 640;
    int model_height = 640;
    cap.set(cv::CAP_PROP_FRAME_WIDTH, model_width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, model_height);

    // 初始化模型
    rknn_app_context_t rknn_app_ctx;
    memset(&rknn_app_ctx, 0, sizeof(rknn_app_context_t));
    int ret = init_yolov8_model(model_path, &rknn_app_ctx);
    if (ret != 0)
    {
        printf("init_yolov8_model fail! ret=%d\n", ret);
        return -1;
    }

    init_post_process();

    cv::Mat frame;
    struct timeval start_time, end_time;
    float fps = 0;
    int frame_count = 0;

    // 添加结果保存功能
    int save_count = 0;
    const int SAVE_INTERVAL = 30; // 每30帧保存一次

    while (true)
    {
        gettimeofday(&start_time, NULL);

        if (!cap.read(frame))
        {
            printf("Error: Failed to read frame\n");
            break;
        }

        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);

        image_buffer_t src_image;
        memset(&src_image, 0, sizeof(image_buffer_t));
        src_image.virt_addr = frame.data;
        src_image.width = frame.cols;
        src_image.height = frame.rows;
        src_image.format = IMAGE_FORMAT_RGB888;

        object_detect_result_list od_results;
        ret = inference_yolov8_model(&rknn_app_ctx, &src_image, &od_results);
        if (ret != 0)
        {
            printf("inference_yolov8_model fail! ret=%d\n", ret);
            continue;
        }

        // 控制台输出检测结果
        printf("\n------ Frame %d ------\n", frame_count);
        for (int i = 0; i < od_results.count; i++)
        {
            object_detect_result *det_result = &(od_results.results[i]);
            printf("[%s] Box(%d,%d,%d,%d) Conf:%.1f%%\n", 
                   coco_cls_to_name(det_result->cls_id),
                   det_result->box.left, det_result->box.top,
                   det_result->box.right, det_result->box.bottom,
                   det_result->prop * 100);
        }

        // 定期保存带标注的图像
        if (save_count % SAVE_INTERVAL == 0)
        {
            cv::Mat output_frame;
            cv::cvtColor(frame, output_frame, cv::COLOR_RGB2BGR);
            char filename[64];
            sprintf(filename, "result_%04d.jpg", save_count/SAVE_INTERVAL);
            cv::imwrite(filename, output_frame);
            printf("Saved result to %s\n", filename);
        }
        save_count++;

        // 计算帧率
        gettimeofday(&end_time, NULL);
        float time_use = (end_time.tv_sec - start_time.tv_sec) * 1000 + 
                        (end_time.tv_usec - start_time.tv_usec) / 1000.0;
        fps = 1000.0 / time_use;
        printf("Current FPS: %.2f\n", fps);

        frame_count++;
    }

    // 释放资源
    cap.release();
    release_yolov8_model(&rknn_app_ctx);
    deinit_post_process();

    return 0;
}
