import argparse
import json
import os
from pathlib import Path


def main():
    parser=argparse.ArgumentParser(description='Component OBB / segmentation CAD workbench')
    sub=parser.add_subparsers(dest='command',required=True)
    check=sub.add_parser('check',help='Check the built workbench runtime without opening a window')
    check.add_argument('--task',choices=['obb','segment'],default='obb')
    gui=sub.add_parser('gui');gui.add_argument('--task',choices=['obb','segment'],default='obb');gui.add_argument('--project')
    export=sub.add_parser('dataset'); export.add_argument('project'); export.add_argument('output')
    train=sub.add_parser('train'); train.add_argument('dataset'); train.add_argument('task',choices=['obb','segment']); train.add_argument('output')
    train.add_argument('--epochs',type=int,default=100); train.add_argument('--imgsz',type=int,default=640)
    train.add_argument('--device',default='cpu'); train.add_argument('--batch',type=int,default=4)
    deploy=sub.add_parser('export'); deploy.add_argument('weights'); deploy.add_argument('dataset'); deploy.add_argument('task',choices=['obb','segment']); deploy.add_argument('output')
    infer=sub.add_parser('infer'); infer.add_argument('manifest'); infer.add_argument('image'); infer.add_argument('output')
    infer.add_argument('--roi',nargs=4,type=int); infer.add_argument('--confidence',type=float,default=.5);infer.add_argument('--tile',type=int,default=1024)
    args=parser.parse_args()
    root=Path(os.environ.get('VISION_CAD_STATE_DIR',str(Path(__file__).resolve().parents[2]/'build')))
    os.environ.setdefault('YOLO_CONFIG_DIR',str(root/'pcb-dl-settings'))
    os.environ.setdefault('MPLCONFIGDIR',str(root/'pcb-dl-matplotlib'))
    os.environ.setdefault('TORCH_HOME',str(root/'pcb-dl-torch'))
    os.environ.setdefault('YOLO_AUTOINSTALL','False')
    os.environ.setdefault('OMP_NUM_THREADS','4')
    if args.command=='check':
        import importlib.metadata
        from .window import Workbench
        from PySide6.QtWidgets import QApplication
        app=QApplication.instance() or QApplication([]);window=Workbench();window.task.setCurrentText(args.task)
        print(json.dumps(dict(status='ok',theme=window.theme_mode,task=window.task.currentText(),steps=window.pages.count(),module=str(Path(__file__).resolve()),
            versions={p:importlib.metadata.version(p) for p in ('PySide6','onnxruntime','torch','ultralytics')})))
        window.deleteLater();return 0
    if args.command=='gui':
        from .window import run
        return run(args.task,args.project)
    if args.command=='dataset':
        from .dataset import export_dataset
        print(export_dataset(json.loads(Path(args.project).read_text(encoding='utf-8-sig')),args.output))
    elif args.command=='train':
        from .training import train
        print(train(args.dataset,args.task,args.output,args.epochs,args.imgsz,args.device,args.batch))
    elif args.command=='export':
        from .training import export_model
        print(export_model(args.weights,args.dataset,args.task,args.output))
    else:
        from .inference import LearningEngine
        from .model import atomic_json,read_image,make_recipe
        image=read_image(args.image); h,w=image.shape[:2]; engine=LearningEngine(args.manifest)
        objects=engine.predict(image,args.roi or [0,0,w,h],args.confidence,args.tile)
        atomic_json(args.output,make_recipe(args.image,w,h,engine.spec['profiles'],objects,engine.spec))


if __name__=='__main__':
    raise SystemExit(main())
