"""Synthetic pipeline fixture only; never a real PCB accuracy benchmark."""
import argparse
import random
from pathlib import Path

import cv2
import numpy as np

from .dataset import export_dataset
from .model import CLASSES,atomic_json,corners,new_project


def generate(destination,count=12):
    root=Path(destination).resolve();root.mkdir(parents=True,exist_ok=True)
    project=new_project();project['demo_only']=True
    project['profiles']={name:{'w':w,'h':h,'window_ratio':.6} for name,w,h in zip(CLASSES,[28,24,48],[12,18,36])}
    rng=random.Random(42)
    for index in range(count):
        image=np.full((256,256,3),(35+index,65,30),dtype=np.uint8);labels=[]
        for c,name in enumerate(CLASSES):
            x=48+c*77+rng.randrange(-6,7); y=90+rng.randrange(-24,25)
            shape={**project['profiles'][name],'x':x,'y':y,'angle':rng.randrange(-75,76)}
            points=corners(shape);color=[(65,85,150),(150,150,80),(35,35,35)][c]
            cv2.fillConvexPoly(image,np.int32(points),color)
            cv2.polylines(image,[np.int32(points)],True,(200,200,200),2)
            labels.append({'class':name,'points':points})
        path=root/f'demo-{index:03}.png';cv2.imencode('.png',image)[1].tofile(path)
        split='train' if index<count-4 else ('val' if index<count-2 else 'test')
        project['images'].append({'path':str(path),'width':256,'height':256,'group':f'synthetic-board-{index}',
                                  'split':split,'reviewed':True,'annotations':labels})
    atomic_json(root/'project.json',project)
    export_dataset(project,root/'dataset')
    return root/'project.json'


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('destination');p.add_argument('--count',type=int,default=12)
    a=p.parse_args();print(generate(a.destination,a.count))
