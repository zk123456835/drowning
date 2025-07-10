#!/usr/bin/python3
from http.server import HTTPServer, BaseHTTPRequestHandler
import cv2
import time
import numpy as np
from rknnlite.api import RKNNLite

# 配置参数
CAMERA_ID = 0  # 默认摄像头ID
MODEL_PATH = 'yolov8.rknn'  # RKNN转换后的模型路径
CLASS_NAMES = ['person', 'bicycle', 'car', 'motorcycle', 'airplane', 'bus', 'train', 'truck', 'boat', 
              'traffic light', 'fire hydrant', 'stop sign', 'parking meter', 'bench', 'bird', 'cat',
              'dog', 'horse', 'sheep', 'cow', 'elephant', 'bear', 'zebra', 'giraffe', 'backpack',
              'umbrella', 'handbag', 'tie', 'suitcase', 'frisbee', 'skis', 'snowboard', 'sports ball',
              'kite', 'baseball bat', 'baseball glove', 'skateboard', 'surfboard', 'tennis racket', 'bottle',
              'wine glass', 'cup', 'fork', 'knife', 'spoon', 'bowl', 'banana', 'apple', 'sandwich', 'orange',
              'broccoli', 'carrot', 'hot dog', 'pizza', 'donut', 'cake', 'chair', 'couch', 'potted plant',
              'bed', 'dining table', 'toilet', 'tv', 'laptop', 'mouse', 'remote', 'keyboard', 'cell phone',
              'microwave', 'oven', 'toaster', 'sink', 'refrigerator', 'book', 'clock', 'vase', 'scissors',
              'teddy bear', 'hair drier', 'toothbrush']  # COCO类别名称

# 模型输入尺寸
INPUT_SIZE = 640
CONF_THRESHOLD = 0.5  # 置信度阈值
NMS_THRESHOLD = 0.5   # NMS阈值

# 初始化RKNN
print("正在加载RKNN模型...")
rknn = RKNNLite()

# 加载模型
ret = rknn.load_rknn(MODEL_PATH)
if ret != 0:
    print(f"加载RKNN模型失败，错误代码: {ret}")
    exit(ret)

# 初始化运行时环境 - 使用NPU核心0和1
print("初始化RKNN运行时...")
ret = rknn.init_runtime(core_mask=RKNNLite.NPU_CORE_0_1)
if ret != 0:
    print(f"初始化运行时失败，错误代码: {ret}")
    rknn.release()
    exit(ret)

print("模型加载完成!")

def sigmoid(x):
    return 1 / (1 + np.exp(-x))

def post_process(outputs, img_shape):
    """
    后处理函数：解析YOLOv8输出
    """
    boxes, scores, class_ids = [], [], []
    original_h, original_w = img_shape[:2]
    
    # 解析每个输出层
    for output in outputs:
        # output形状: [1, 84, 8400]
        output = output.reshape([84, -1])
        
        # 分离边界框和类别分数
        bbox_raw = output[:4, :]   # [4, 8400]
        scores_raw = output[4:4+80, :]  # [80, 8400]
        
        # 计算检测分数和类别
        class_id = np.argmax(scores_raw, axis=0)
        max_scores = np.max(scores_raw, axis=0)
        
        # 筛选置信度高于阈值的检测
        valid_indices = max_scores > CONF_THRESHOLD
        bbox_raw = bbox_raw[:, valid_indices]
        scores_raw = scores_raw[:, valid_indices]
        max_scores = max_scores[valid_indices]
        class_id = class_id[valid_indices]
        
        # 转换边界框格式 (cx, cy, w, h) -> (x1, y1, x2, y2)
        cx, cy, w, h = bbox_raw
        x1 = (cx - w / 2) * (original_w / INPUT_SIZE)
        y1 = (cy - h / 2) * (original_h / INPUT_SIZE)
        x2 = (cx + w / 2) * (original_w / INPUT_SIZE)
        y2 = (cy + h / 2) * (original_h / INPUT_SIZE)
        
        boxes.extend(np.stack([x1, y1, x2, y2], axis=1).tolist())
        scores.extend(max_scores.tolist())
        class_ids.extend(class_id.tolist())
    
    # 应用NMS
    if len(boxes) > 0:
        indices = cv2.dnn.NMSBoxes(boxes, scores, CONF_THRESHOLD, NMS_THRESHOLD)
        final_boxes = [boxes[i] for i in indices]
        final_scores = [scores[i] for i in indices]
        final_class_ids = [class_ids[i] for i in indices]
        return final_boxes, final_scores, final_class_ids
    
    return [], [], []

class StreamingHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/':
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            
            # 包含实时视频和检测结果的HTML页面
            html = """
            <html>
            <head>
                <title>RK3588 YOLOv8实时检测</title>
                <style>
                    body { 
                        text-align: center; 
                        background-color: #121212; 
                        color: white; 
                        font-family: Arial, sans-serif;
                    }
                    .container {
                        max-width: 800px;
                        margin: 0 auto;
                        padding: 20px;
                    }
                    h1 { 
                        margin-top: 10px;
                        color: #4CAF50;
                    }
                    #stats {
                        display: flex;
                        justify-content: center;
                        gap: 20px;
                        margin: 15px 0;
                        font-size: 18px;
                    }
                    .stat-box {
                        background: rgba(30, 30, 30, 0.7);
                        padding: 10px 20px;
                        border-radius: 8px;
                        min-width: 120px;
                    }
                    .video-container {
                        position: relative;
                        display: inline-block;
                    }
                    #fpsCounter {
                        position: absolute; 
                        top: 15px; 
                        right: 15px; 
                        background: rgba(0, 0, 0, 0.7); 
                        padding: 8px 15px;
                        border-radius: 20px;
                        font-family: monospace;
                        font-size: 18px;
                        color: #FFD700;
                    }
                    #detectionCounter {
                        position: absolute; 
                        top: 15px; 
                        left: 15px; 
                        background: rgba(0, 0, 0, 0.7); 
                        padding: 8px 15px;
                        border-radius: 20px;
                        font-family: monospace;
                        font-size: 18px;
                        color: #FF6347;
                    }
                    img {
                        border-radius: 8px;
                        box-shadow: 0 4px 20px rgba(0, 0, 0, 0.5);
                    }
                </style>
            </head>
            <body>
                <div class="container">
                    <h1>YOLOv8 实时对象检测 (RK3588 NPU加速)</h1>
                    
                    <div class="video-container">
                        <img src="/video_feed" width="640" height="480">
                        <div id="fpsCounter">FPS: --</div>
                        <div id="detectionCounter">检测数: 0</div>
                    </div>
                    
                    <div id="stats">
                        <div class="stat-box">模型: YOLOv8</div>
                        <div class="stat-box">输入尺寸: 640×640</div>
                        <div class="stat-box">NPU核心: 0+1</div>
                    </div>
                </div>
                
                <script>
                    // 性能监控
                    const fpsCounter = document.getElementById('fpsCounter');
                    const detectionCounter = document.getElementById('detectionCounter');
                    let lastFrameTime = Date.now();
                    let frameCount = 0;
                    
                    const img = document.querySelector('img');
                    img.onload = function() {
                        frameCount++;
                        const now = Date.now();
                        const elapsed = now - lastFrameTime;
                        
                        if (elapsed >= 1000) {
                            const fps = Math.round(frameCount * 1000 / elapsed);
                            fpsCounter.textContent = `FPS: ${fps}`;
                            frameCount = 0;
                            lastFrameTime = now;
                        }
                    };
                    
                    // 通过SSE获取检测统计
                    const eventSource = new EventSource('/stats');
                    eventSource.onmessage = function(event) {
                        const data = JSON.parse(event.data);
                        detectionCounter.textContent = `检测数: ${data.detections}`;
                    };
                </script>
            </body>
            </html>
            """
            self.wfile.write(html.encode('utf-8'))
            
        elif self.path == '/stats':
            # 发送服务器端事件(SSE)用于实时统计
            self.send_response(200)
            self.send_header('Content-type', 'text/event-stream')
            self.send_header('Cache-Control', 'no-cache')
            self.send_header('Connection', 'keep-alive')
            self.end_headers()
            
            try:
                while True:
                    # 发送当前检测数
                    data = f"data: {{\"detections\": {self.server.detection_count}}}\n\n"
                    self.wfile.write(data.encode('utf-8'))
                    self.wfile.flush()
                    time.sleep(0.5)  # 每0.5秒更新一次
            except (BrokenPipeError, ConnectionResetError):
                pass  # 客户端断开连接
            
        elif self.path == '/video_feed':
            self.send_response(200)
            self.send_header('Content-type', 'multipart/x-mixed-replace; boundary=frame')
            self.end_headers()
            
            camera = cv2.VideoCapture(CAMERA_ID)
            camera.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
            camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
            
            try:
                while True:
                    start_time = time.time()
                    ret, frame = camera.read()
                    if not ret:
                        break
                    
                    # 记录原始尺寸
                    original_h, original_w = frame.shape[:2]
                    
                    # 预处理 - 调整大小并转换为RGB
                    img_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                    img_resized = cv2.resize(img_rgb, (INPUT_SIZE, INPUT_SIZE))
                    
                    # 推理
                    outputs = rknn.inference(inputs=[img_resized])
                    
                    # 后处理
                    boxes, scores, class_ids = post_process(outputs, (original_h, original_w))
                    self.server.detection_count = len(boxes)  # 更新检测计数
                    
                    # 绘制检测结果
                    for box, score, class_id in zip(boxes, scores, class_ids):
                        x1, y1, x2, y2 = map(int, box)
                        
                        # 绘制边界框
                        color = (0, 255, 0)  # 绿色
                        cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
                        
                        # 绘制标签和置信度
                        label = f"{CLASS_NAMES[class_id]}: {score:.2f}"
                        cv2.putText(frame, label, (x1, y1 - 10), 
                                   cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2)
                    
                    # 编码并发送帧
                    _, jpeg = cv2.imencode('.jpg', frame)
                    self.wfile.write(b'--frame\r\n')
                    self.send_header('Content-type', 'image/jpeg')
                    self.send_header('Content-length', len(jpeg))
                    self.end_headers()
                    self.wfile.write(jpeg.tobytes())
                    self.wfile.write(b'\r\n')
                    
                    # 计算处理时间并控制帧率
                    processing_time = time.time() - start_time
                    time.sleep(max(0.01, 0.033 - processing_time))  # 目标~30FPS
            
            except ConnectionAbortedError:
                print("客户端断开连接")
            except Exception as e:
                print(f"处理错误: {str(e)}")
            finally:
                camera.release()

class StreamingServer(HTTPServer):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.detection_count = 0  # 用于存储当前检测数

def main():
    server = StreamingServer(('0.0.0.0', 8000), StreamingHandler)
    print("YOLOv8视频流服务器已启动，访问 http://<RK3588_IP>:8000")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n正在关闭服务器...")
        rknn.release()
        server.server_close()

if __name__ == '__main__':
    main()