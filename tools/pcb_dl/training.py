"""Explicit local train / evaluate / ONNX export pipeline."""
import hashlib
import json
from pathlib import Path

from .model import atomic_json, validate_profiles
from .dataset import verify_dataset


def train(dataset, task, output, epochs=100, imgsz=640, device='cpu', batch=4):
    from ultralytics import YOLO
    dataset=Path(dataset).resolve(); output=Path(output).resolve()
    if task not in ('obb','segment') or epochs<1 or imgsz<64:
        raise ValueError('訓練設定無效')
    if not (dataset/'source-project.json').is_file():
        raise ValueError('請用工作台匯出的審核資料訓練')
    if (output/task).exists():
        raise ValueError('此 task 的輸出目錄已存在，請選新的輸出目錄，避免覆寫或讀錯權重')
    verify_dataset(dataset)
    source=json.loads((dataset/'source-project.json').read_text(encoding='utf-8'))
    validate_profiles(source['profiles'],source['classes'])
    suffix='obb' if task=='obb' else 'seg'
    # Random initialization is explicit: no COCO/DOTA class is presented as a PCB class.
    model=YOLO(f'yolo11n-{suffix}.yaml',task=task)
    model.train(data=str(dataset/task/'data.yaml'),epochs=epochs,imgsz=imgsz,batch=batch,device=device,
                project=str(output),name=task,exist_ok=False,workers=0,pretrained=False,seed=42,
                plots=False,amp=False,cache=False,save=True)
    return export_model(output/task/'weights/best.pt',dataset,task,output/task/'deployment',imgsz,device)


def export_model(weights,dataset,task,output,imgsz=640,device='cpu'):
    import shutil
    import ultralytics
    from ultralytics import YOLO
    dataset=Path(dataset).resolve(); output=Path(output).resolve(); output.mkdir(parents=True,exist_ok=True)
    project=json.loads((dataset/'source-project.json').read_text(encoding='utf-8'))
    dataset_digest=verify_dataset(dataset)
    validate_profiles(project['profiles'],project['classes'])
    model=YOLO(str(weights),task=task)
    if model.task!=task or [model.names[i] for i in range(len(model.names))]!=project['classes']:
        raise ValueError('權重 task／類別與資料集不一致')
    metrics=model.val(data=str(dataset/task/'data.yaml'),split='test',device=device,imgsz=imgsz,plots=False,
                      project=str(output),name='heldout-evaluation',workers=0)
    atomic_json(output/'metrics.json',{'split':'test','metrics':{k:float(v) for k,v in metrics.results_dict.items()}})
    file=Path(model.export(format='onnx',imgsz=imgsz,opset=17,simplify=False,dynamic=False,device='cpu'))
    target=output/'model.onnx'
    if file.resolve()!=target.resolve(): shutil.copyfile(file,target)
    spec={'format':'pcb-dl-model','version':1,'task':task,'model':target.name,'imgsz':imgsz,
          'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'classes':project['classes'],
          'profiles':project['profiles'],'demo_only':project.get('demo_only',False),
          'training':{'ultralytics':ultralytics.__version__,'dataset_sha256':dataset_digest,
                      'initialization':'random','heldout_split':'test','weights':str(Path(weights).resolve())}}
    atomic_json(output/'model.json',spec)
    return output/'model.json'
