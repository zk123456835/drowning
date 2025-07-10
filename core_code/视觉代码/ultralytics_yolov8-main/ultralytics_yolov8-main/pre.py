# from ultralytics import YOLO
# import os
# os.environ['KMP_DUPLICATE_LIB_OK'] = 'TRUE'  # 跳过 OMP 冲突警告
# if __name__ == '__main__':
#     model = YOLO(model="./runs/detect/train7/weights/best.pt") # load a custom model
#     metrics=model.val()
# Predict with the model
# results = model("./datasets/drowning/images/train/12_mp4-17_jpg.rf.1e169b9a23bb66edc4f0fe4516a16c8e.jpg",save=True)
from ultralytics import YOLO
model = YOLO("./runs/detect/train2/weights/best.pt", task="detect")
result = model(source="datasets/drowning/images/train/drowning_10_0_1856_jpeg_jpg.rf.0790fc547cde0e9d0cadf0a2db2f1ef3.jpg", save=True)