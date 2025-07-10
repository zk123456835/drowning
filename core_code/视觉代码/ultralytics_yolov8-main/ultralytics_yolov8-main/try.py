# from ultralytics import YOLO
# yolo = YOLO("./yolov8n.pt", task="detect")
# result = yolo(source="./ultralytics/assets/bus.jpg", save=True)

#

from ultralytics import YOLO

# import os
# os.environ['KMP_DUPLICATE_LIB_OK'] = 'TRUE'  # 跳过 OMP 冲突警告
if __name__ == '__main__':
    model=YOLO(model="yolov8n.pt")

    model.train(data = "drowning.yaml",
                    workers=0,
                    epochs=150,
                    batch=4)
# from ultralytics import YOLO
#
#
# model=YOLO(model="runs/detect/train3/weights/best.pt")
# model.export(format="rknn")