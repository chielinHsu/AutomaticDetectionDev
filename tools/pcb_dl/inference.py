"""Actual ONNX inference, overlapping tiles, rotated NMS and fixed CAD profiles."""
import hashlib
import ast
import json
from pathlib import Path

import cv2
import numpy as np

from .model import corners, inside, pose_from_polygon, validate_profiles
from .inspection import fits_inspection,with_inspection


def tile_origins(length, size, overlap):
    if length<=size: return [0]
    stride=max(1,int(size*(1-overlap)))
    return sorted(set(list(range(0,length-size+1,stride))+[length-size]))


def iou(a,b):
    ap=np.asarray(corners(a['observed']),dtype=np.float32)
    bp=np.asarray(corners(b['observed']),dtype=np.float32)
    area,_=cv2.intersectConvexConvex(ap,bp)
    union=cv2.contourArea(ap)+cv2.contourArea(bp)-area
    return area/union if union>0 else 0


def suppress(objects, threshold=.35):
    out=[]
    for obj in sorted(objects,key=lambda v:-v['confidence']):
        if not any(obj['class']==other['class'] and iou(obj,other)>threshold for other in out):
            out.append(obj)
    return out


class LearningEngine:
    def __init__(self, manifest_file, profiles=None):
        from ultralytics import YOLO
        manifest_file=Path(manifest_file).resolve()
        spec=json.loads(manifest_file.read_text(encoding='utf-8-sig'))
        if spec.get('format')!='pcb-dl-model' or spec.get('version')!=1 or spec.get('task') not in ('obb','segment'):
            raise ValueError('模型 manifest 格式或 task 無效')
        self.task=spec['task']; self.spec=spec
        model=(manifest_file.parent/spec['model']).resolve()
        if model.suffix.lower()!='.onnx' or not model.is_file():
            raise ValueError('推論必須使用本機 ONNX 模型')
        if hashlib.sha256(model.read_bytes()).hexdigest()!=spec['sha256']:
            raise ValueError('模型 SHA-256 與 manifest 不一致')
        import onnx
        metadata={v.key:v.value for v in onnx.load(str(model),load_external_data=False).metadata_props}
        if metadata.get('task')!=self.task:
            raise ValueError('ONNX task 與 manifest 不一致')
        names=ast.literal_eval(metadata.get('names','{}'))
        actual=[str(names[i]) for i in range(len(names))] if isinstance(names,dict) else list(names)
        if actual!=spec['classes']:raise ValueError('ONNX 類別與 manifest 不一致')
        validate_profiles(spec['profiles'],spec['classes'])
        if profiles is not None:
            validate_profiles(profiles,spec['classes'])
            spec={**spec,'profiles':{name:profiles[name].copy() for name in spec['classes']},'cad_override':True}
            self.spec=spec
        self.model=YOLO(str(model),task=self.task)

    def predict(self,image,roi,confidence=.5,tile=1024,overlap=.25,cancel=lambda:False,progress=lambda *_:None):
        if not .05<=confidence<=1 or not 64<=tile<=4096 or not 0<=overlap<.75:
            raise ValueError('推論參數超出範圍')
        height,width=image.shape[:2]
        x,y,w,h=map(int,roi)
        if min(w,h)<=0 or x<0 or y<0 or x+w>width or y+h>height:
            raise ValueError('ROI 超出影像')
        positions=[(x+xx,y+yy) for yy in tile_origins(h,tile,overlap) for xx in tile_origins(w,tile,overlap)]
        if len(positions)>4096: raise ValueError('分塊數超過 4096')
        all_objects=[]
        for index,(tx,ty) in enumerate(positions):
            if cancel(): raise InterruptedError('推論已取消')
            crop=image[ty:min(ty+tile,y+h),tx:min(tx+tile,x+w)]
            result=self.model.predict(crop,conf=confidence,imgsz=self.spec.get('imgsz',640),device='cpu',verbose=False,max_det=1000)[0]
            actual=[str(result.names[i]) for i in range(len(result.names))]
            if actual!=self.spec['classes']: raise ValueError('模型類別與 manifest 不一致')
            if self.task=='obb':
                data=[] if result.obb is None else zip(result.obb.xyxyxyxy.cpu().numpy(),result.obb.cls.cpu().numpy(),result.obb.conf.cpu().numpy())
            else:
                data=[] if result.masks is None else zip(result.masks.xy,result.boxes.cls.cpu().numpy(),result.boxes.conf.cpu().numpy())
            for points,cls,score in data:
                points=np.asarray(points,dtype=np.float32)+[tx,ty]
                if len(points)<3: continue
                observed=pose_from_polygon(points); name=actual[int(cls)]
                # Reject clipped edge contours; overlap offers a complete observation in the adjacent tile.
                local=points-[tx,ty]; ch,cw=crop.shape[:2]
                clipped=(tx>x and local[:,0].min()<=1) or (ty>y and local[:,1].min()<=1) or (tx+cw<x+w and local[:,0].max()>=cw-2) or (ty+ch<y+h and local[:,1].max()>=ch-2)
                if clipped: continue
                profile=self.spec['profiles'][name]
                pose={**observed,'w':profile['w'],'h':profile['h']}
                if not inside(corners(pose),roi): continue
                if not fits_inspection(pose,profile,roi):continue
                all_objects.append(with_inspection({'class':name,'pose':pose,'observed':observed,'outline':points.tolist(),
                                    'confidence':float(score),'manual':False},profile))
            progress(index+1,len(positions))
            if len(all_objects)>20000: raise ValueError('候選過多，請縮小 ROI 或提高門檻')
        result=suppress(all_objects)
        if len(result)>5000:raise ValueError('超過 5000 個 CAD，請縮小 ROI')
        return result
