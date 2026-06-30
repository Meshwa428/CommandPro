from huggingface_hub import hf_hub_download
from ultralytics import YOLO

# Download the model
# model_path = hf_hub_download(
#     repo_id="macpaw-research/yolov11l-ui-elements-detection",
#     filename="ui-elements-detection.pt",
# )

model_path = "./ui_element_detection/yolo_models/my-best.pt"

# Load and run prediction
model = YOLO(model_path)

# set model parameters
#model.overrides['conf'] = 0.25  # NMS confidence threshold
#model.overrides['iou'] = 0.45  # NMS IoU threshold
#model.overrides['agnostic_nms'] = False  # NMS class-agnostic
#model.overrides['max_det'] = 1000  # maximum number of detections per image



results = model.predict("./test.png")

# Display result
results[0].show()
