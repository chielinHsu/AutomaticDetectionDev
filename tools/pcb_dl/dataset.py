"""One reviewed polygon source exports both OBB and instance segmentation datasets."""
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np
import yaml

from .model import atomic_json, polygon, pose_from_polygon, corners, read_image, validate_project


def export_dataset(project, destination):
    counts=validate_project(project, require_review=True)
    if any(counts[s]==0 for s in ("train","val","test")):
        raise ValueError("至少需要三個獨立 group，分別作 train、val 與 test")
    destination=Path(destination).resolve()
    if destination.exists() and any(destination.iterdir()):
        raise ValueError("匯出目錄必須為空，避免混入舊標註")
    class_ids={name:i for i,name in enumerate(project["classes"])}
    for task in ("obb","segment"):
        root=destination/task
        for split in counts:
            (root/'images'/split).mkdir(parents=True)
            (root/'labels'/split).mkdir(parents=True)
        for entry in project["images"]:
            source=read_image(entry["path"]); h,w=source.shape[:2]
            key=hashlib.sha256((str(Path(entry["path"]).resolve())).encode()).hexdigest()[:16]
            image_file=root/'images'/entry["split"]/(key+'.png')
            ok,data=cv2.imencode('.png',source)
            if not ok: raise ValueError("影像輸出失敗")
            data.tofile(image_file)
            lines=[]
            for label in entry["annotations"]:
                points=polygon(label["points"])
                if task=='obb': points=np.asarray(corners(pose_from_polygon(points)))
                normalized=np.clip(points/[w,h],0,1).ravel()
                lines.append(str(class_ids[label["class"]])+' '+' '.join(f'{v:.8f}' for v in normalized))
            (root/'labels'/entry["split"]/(key+'.txt')).write_text('\n'.join(lines)+'\n',encoding='utf-8')
        config={"path":str(root),"train":"images/train","val":"images/val","test":"images/test","names":project["classes"]}
        (root/'data.yaml').write_text(yaml.safe_dump(config,allow_unicode=True),encoding='utf-8')
    atomic_json(destination/'source-project.json',project)
    atomic_json(destination/'split-audit.json',{"counts":counts,"groups":{e['group']:e['split'] for e in project['images']}})
    files={str(p.relative_to(destination)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest()
           for p in destination.rglob('*') if p.is_file()}
    atomic_json(destination/'dataset-files.json',files)
    return destination


def verify_dataset(destination):
    root=Path(destination).resolve();manifest=root/'dataset-files.json'
    if not manifest.is_file():raise ValueError('缺少 dataset-files.json，請重新匯出審核資料')
    files=json.loads(manifest.read_text(encoding='utf-8'))
    for name,digest in files.items():
        path=(root/name).resolve()
        if not path.is_relative_to(root) or not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=digest:
            raise ValueError('匯出資料已變更或遺失：'+name)
    actual={str(p.relative_to(root)).replace('\\','/') for p in root.rglob('*') if p.is_file() and p.suffix in ('.png','.txt','.yaml','.json') and p!=manifest}
    if actual!=set(files):raise ValueError('資料集包含未審核的新增檔案')
    return hashlib.sha256(manifest.read_bytes()).hexdigest()
