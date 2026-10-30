"""Import explicit multi-angle RGB capture sets without treating views as independent boards."""
import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from .model import atomic_json,new_project,validate_project

CHANNEL_FILES=('high-angle-red_cam0.bmp','middle-angle-green_cam0.bmp','low-angle-blue_cam0.bmp')


def import_capture(root,destination,group):
    root=Path(root).resolve();destination=Path(destination).resolve()
    if not root.is_dir() or not group.strip():raise ValueError('需要存在的影像目錄與非空來源 group')
    if destination.exists() and any(destination.iterdir()):raise ValueError('輸出目錄必須為空，避免覆寫人工標註')
    if destination==root or root in destination.parents:raise ValueError('請把輸出放在原始影像目錄之外')
    project=new_project();audit=[]
    folders=sorted({p.parent for file_name in (*CHANNEL_FILES,'*.png') for p in root.rglob(file_name)})
    if not folders:raise ValueError('找不到 high-angle-red_cam0.bmp 通道組')
    for folder in folders:
        combined=list(folder.glob('*.png'))
        if len(combined)!=1:raise ValueError(f'{folder} 必須有一張明確的合併 PNG')
        raw=cv2.imdecode(np.fromfile(combined[0],dtype=np.uint8),cv2.IMREAD_UNCHANGED)
        if raw is None or raw.dtype!=np.uint8 or raw.ndim!=3 or raw.shape[2]!=3:
            raise ValueError('合併圖必須是 8-bit 三通道影像')
        h,w=raw.shape[:2];checks={};paths={}
        for name,bgr_index,file_name in zip(('R','G','B'),(2,1,0),CHANNEL_FILES):
            path=folder/file_name
            if not path.is_file():raise ValueError('缺少通道：'+str(path))
            channel=cv2.imdecode(np.fromfile(path,dtype=np.uint8),cv2.IMREAD_UNCHANGED)
            if channel is None or channel.dtype!=np.uint8 or channel.ndim!=2 or channel.shape!=(h,w):
                raise ValueError('通道格式／尺寸不符：'+str(path))
            difference=np.abs(raw[:,:,bgr_index].astype(np.int16)-channel.astype(np.int16))
            checks[name]={'exact_match':bool(np.array_equal(raw[:,:,bgr_index],channel)),
                          'mae':float(difference.mean()),'max_difference':int(difference.max())}
            paths[name]=str(path)
        if not all(c['exact_match'] for c in checks.values()):
            raise ValueError('合併圖與來源通道不一致，需先檢查合併或前處理：'+str(combined[0]))
        project['images'].append({'path':str(combined[0]),'width':w,'height':h,'group':group,
                                  'split':'train','reviewed':False,'annotations':[],
                                  'capture_view':folder.name,'channel_sources':paths})
        audit.append({'view':folder.name,'combined':str(combined[0]),'width':w,'height':h,'channels':checks})
    counts=validate_project(project)
    report={'format':'pcb-capture-audit','version':1,'root':str(root),'group':group,'views':audit,
            'counts':counts,'labels':0,'reviewed':False,'independent_groups':1,
            'note':'單一來源群組，不能以不同通道或 Z 視圖互作獨立 train/val/test；尚未標註或訓練。'}
    destination.mkdir(parents=True,exist_ok=True)
    atomic_json(destination/'project.json',project);atomic_json(destination/'audit.json',report)
    (destination/'README.md').write_text('''# PCB 真實圖標註起始專案

在 PCB 深度學習工作台按「開啟標註」，載入 project.json。
此專案只引用原始合併 PNG；沒有更動或複製原圖，也沒有自動標註。
R/G/B 來源與逐像素比對結果在 audit.json。不要把不同通道當成獨立 PCB。

目前所有影像歸於同一來源 group，split 暫設 train，reviewed=false。
請逐張標註並人工審核；不同 Z 圖的座標可能改變，不應直接複製輪廓。
CAD 尺寸尚未設定，請依封裝與解析度設定。
目前不可匯出正式 train/val/test 資料：還需要獨立板子／拍攝批次的分組與標註。
單板裁塊或不同 Z 可供開發實驗，但不能據此宣稱跨板泛化品質。
請將人工標註另存到持久資料夾，避免存放在可能清理的 build 內。
''',encoding='utf-8',newline='\n')
    return destination/'project.json',report


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('root');parser.add_argument('destination')
    parser.add_argument('--group',required=True,help='Same board/capture group for all imported views')
    args=parser.parse_args();project,report=import_capture(args.root,args.destination,args.group)
    print(json.dumps({'project':str(project),'views':len(report['views']),'groups':1}))
