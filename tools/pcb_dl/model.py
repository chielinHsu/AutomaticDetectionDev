"""Versioned, validated local documents. Angles are screen counterclockwise degrees."""
import copy
import hashlib
import json
import math
import os
from pathlib import Path

import cv2
import numpy as np
from .annotation_geometry import annotation_shape
from .inspection import fits_inspection,validate_inspection,with_inspection

CLASSES = ["resistor", "capacitor", "ic"]


def atomic_json(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(data, ensure_ascii=False, indent=2, allow_nan=False), encoding="utf-8")
    os.replace(temporary, path)


def read_image(path):
    raw = cv2.imdecode(np.fromfile(path, dtype=np.uint8), cv2.IMREAD_UNCHANGED)
    if raw is None:
        raise ValueError("無法解碼影像")
    if raw.dtype != np.uint8:
        raw = cv2.normalize(raw, None, 0, 255, cv2.NORM_MINMAX).astype(np.uint8)
    if raw.ndim == 2:
        raw = cv2.cvtColor(raw, cv2.COLOR_GRAY2BGR)
    if raw.shape[2] == 4:
        raw = cv2.cvtColor(raw, cv2.COLOR_BGRA2BGR)
    return raw


def polygon(points):
    p = np.asarray(points, dtype=np.float32)
    if p.ndim != 2 or p.shape[1] != 2 or len(p) < 3 or not np.isfinite(p).all():
        raise ValueError("輪廓至少需要三個有限座標")
    if abs(cv2.contourArea(p)) < 1:
        raise ValueError("輪廓面積不足")
    # Reject self intersections, including non-convex polygon crossings.
    def orient(a, b, c):
        ab,ac=b-a,c-a
        return float(ab[0]*ac[1]-ab[1]*ac[0])
    for i in range(len(p)):
        for j in range(i+2, len(p)):
            if i == 0 and j == len(p)-1:
                continue
            a,b,c,d=p[i],p[(i+1)%len(p)],p[j],p[(j+1)%len(p)]
            if orient(a,b,c)*orient(a,b,d)<0 and orient(c,d,a)*orient(c,d,b)<0:
                raise ValueError("輪廓不可自交")
    return p


def pose_from_polygon(points):
    (x, y), (w, h), clockwise = cv2.minAreaRect(polygon(points))
    if h > w:
        w, h, clockwise = h, w, clockwise+90
    return dict(x=float(x), y=float(y), w=float(w), h=float(h), angle=float((-clockwise+90)%180-90))


def corners(pose, ratio=1):
    return cv2.boxPoints(((pose["x"], pose["y"]), (pose["w"]*ratio, pose["h"]*ratio), -pose["angle"])).tolist()


def inside(points, roi):
    x,y,w,h=roi
    return all(x-1e-3<=px<=x+w+1e-3 and y-1e-3<=py<=y+h+1e-3 for px,py in points)


def new_project():
    return {"format":"pcb-learning-project", "version":1, "classes":CLASSES.copy(), "images":[], "profiles":{}}


def validate_name(obj):
    if not isinstance(obj.get('name',''),str) or len(obj.get('name',''))>128:raise ValueError('元件名稱必須是 128 字元以內的文字')


def validate_project(project, require_review=False):
    if project.get("format")!="pcb-learning-project" or project.get("version")!=1:
        raise ValueError("不支援的標註專案格式")
    names=project["classes"]
    if not names or len(names)!=len(set(names)) or any(not isinstance(n,str) or not n.strip() for n in names):
        raise ValueError("類別必須非空且不重複")
    validate_profiles(project.get('profiles',{}),list(project.get('profiles',{})))
    hashes=set(); groups={}; counts={s:0 for s in ("train","val","test")}
    for entry in project["images"]:
        split=entry["split"]; group=entry["group"]
        if split not in counts or not group.strip():
            raise ValueError("每張圖需設定來源 group 與 train/val/test")
        if group in groups and groups[group]!=split:
            raise ValueError("同一來源 group 不可跨資料集")
        groups[group]=split; counts[split]+=1
        if require_review and not entry.get("reviewed"):
            raise ValueError("請先人工審核每張圖（包括空圖）")
        image=read_image(entry["path"]); h,w=image.shape[:2]
        digest=hashlib.sha256(image.tobytes()).hexdigest()
        if digest in hashes:
            raise ValueError("重複影像，請移除以避免資料洩漏")
        hashes.add(digest)
        if (w,h)!=(entry["width"],entry["height"]):
            raise ValueError("影像尺寸已改變")
        for label in entry["annotations"]:
            annotation_shape(label)
            validate_name(label)
            if label["class"] not in names or not inside(polygon(label["points"]), (0,0,w,h)):
                raise ValueError("標註類別無效或輪廓超出影像")
            profile=project.get('profiles',{}).get(label['class'],{})
            if not fits_inspection(pose_from_polygon(label['points']),profile,(0,0,w,h)):
                raise ValueError('標註的相依檢測框超出影像')
    return counts


def validate_profiles(profiles, classes):
    for name in classes:
        if name not in profiles:
            raise ValueError(f"尚未設定 {name} 的 CAD 規格")
        p=profiles[name]
        if any(not math.isfinite(float(p[k])) or float(p[k])<=0 for k in ("w","h","window_ratio")) or p["window_ratio"]>1:
            raise ValueError("CAD 尺寸須為正值，子窗比例須在 0 到 1")
        if 'inspection' in p:validate_inspection(p['inspection'])


def make_recipe(image_path, width, height, profiles, objects, provenance):
    if not 0<width<=100000 or not 0<height<=100000 or len(objects)>5000:
        raise ValueError("影像尺寸或 CAD 數量超出限制（最多 5000 個）")
    validate_profiles(profiles, list(profiles))
    for obj in objects:
        validate_name(obj)
        if obj["class"] not in profiles:
            raise ValueError("物件沒有對應 CAD")
        p=obj["pose"]
        if any(not math.isfinite(float(p[k])) for k in ("x","y","w","h","angle")) or min(p["w"],p["h"])<=0:
            raise ValueError("非法 CAD 幾何")
        if not inside(corners(p),(0,0,width,height)):
            raise ValueError("CAD 超出影像")
        if not fits_inspection(p,profiles[obj['class']],(0,0,width,height)):
            raise ValueError('CAD 相依檢測框超出影像')
    return {"format":"pcb-dl-cad", "version":1, "image":str(Path(image_path).resolve()), "width":width,"height":height,
            "profiles":copy.deepcopy(profiles),"objects":[with_inspection(o,profiles[o['class']]) for o in objects],"provenance":provenance}
