import cv2

from ultralytics import YOLO

# 加载模型
model = YOLO(model="runs/detect/train4/weights/best.pt")
# 视频文件
video_path = "E:/all/nishui.mp4"

# 打开视频
cap = cv2.VideoCapture(video_path)
#摄像头编号
# camera_no = 0

# 打开摄像头
# cap = cv2.VideoCapture(camera_no)

while cap.isOpened():
    # 获取图像
    res, frame = cap.read()
    # 如果读取成功
    if res:
        # 正向推理
        results = model(frame)

        # 绘制结果
        annotated_frame = results[0].plot()

        # 显示图像
        cv2.imshow(winname= "Drowning", mat=annotated_frame)

        # 按ESC退出
        if cv2.waitKey(1) == 27:
            break

    else:
        break

# 释放链接
cap.release()
# 销毁所有窗口
cv2.destroyAllWindows()



import cv2
from ultralytics import YOLO



# 加载模型
model = YOLO(model="runs/detect/train6/weights/best.pt")
# 视频文件
video_path = "E:/all/one.mp4"

# 打开视频
cap = cv2.VideoCapture(video_path)

# 获取视频的帧率和尺寸
fps = cap.get(cv2.CAP_PROP_FPS)
width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))

# 定义输出视频的编解码器和创建VideoWriter对象
fourcc = cv2.VideoWriter_fourcc(*'mp4v')  # 使用MP4V编码
output_path = "E:/all/one_detected.mp4"  # 输出视频路径
out = cv2.VideoWriter(output_path, fourcc, fps, (width, height))

while cap.isOpened():
    # 获取图像
    res, frame = cap.read()
    # 如果读取成功
    if res:
        # 正向推理
        results = model(frame)

        # 绘制结果
        annotated_frame = results[0].plot()

        # 写入帧到输出视频
        out.write(annotated_frame)

        # 显示图像
        cv2.imshow(winname="Drowning", mat=annotated_frame)

        # 按ESC退出
        if cv2.waitKey(1) == 27:
            break
    else:
        break

# 释放资源
cap.release()
out.release()  # 释放视频写入器
# 销毁所有窗口
cv2.destroyAllWindows()