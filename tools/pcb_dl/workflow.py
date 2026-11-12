import copy
import json
import sys
from pathlib import Path

from PySide6.QtCore import QProcess
from PySide6.QtWidgets import QFileDialog,QInputDialog,QMessageBox

from .model import (atomic_json,corners,inside,make_recipe,new_project,polygon,read_image,validate_profiles,validate_project)
from .annotation_geometry import annotation_shape,is_rectangle,resized_vertex
from .inspection import fits_inspection


class Workflow:
    def entry(self):
        return self.project['images'][self.current] if self.current>=0 else {}

    def idle(self):
        if self.worker is not None and self.worker.isRunning():raise ValueError('請先等待或取消目前推論')
        if self.canvas.points or self.canvas.start is not None:
            raise ValueError('繪製尚未完成：矩形請放開左鍵，輪廓請按「完成輪廓」或 Enter，或按 Esc 取消。')

    def require_image(self):
        self.idle()
        if self.image is None:raise ValueError('請先載入圖片')

    def sync_entry(self,*_):
        if self.current>=0:
            self.entry().update(group=self.group.text(),split=self.split.currentText(),reviewed=self.reviewed.isChecked())
        self.update_guidance()

    def sync_profiles(self):
        profiles={}
        for row,name in enumerate(self.project['classes']):
            values=[self.profiles.item(row,c).text().strip() for c in (1,2,3)]
            if values[0] or values[1]:profiles[name]={**self.project['profiles'].get(name,{}),**dict(zip(['w','h','window_ratio'],map(float,values)))}
        validate_profiles(profiles,list(profiles));self.project['profiles']=profiles

    def add_image(self):
        self.idle()
        files,_=QFileDialog.getOpenFileNames(self,'加入 元件影像','','Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)')
        self.sync_entry()
        for file in files:
            path=str(Path(file).resolve())
            if any(e['path']==path for e in self.project['images']):continue
            image=read_image(file);h,w=image.shape[:2]
            self.project['images'].append(dict(path=path,width=w,height=h,group=Path(file).stem,split='train',reviewed=False,annotations=[]))
        self.reload_images(len(self.project['images'])-1)

    def reload_images(self,index=0):
        self.images.blockSignals(True);self.images.clear()
        self.images.addItems([Path(e['path']).name for e in self.project['images']]);self.images.setCurrentIndex(index);self.images.blockSignals(False)
        self.current=-1
        if self.project['images']:self.switch_image(index)

    def switch_image(self,index):
        self.idle()
        if index<0:return
        if self.canvas.points or self.canvas.start is not None:raise ValueError('請先完成輪廓或按 Esc 取消，再切換圖片。')
        self.sync_entry();self.current=index;self.selected_index=-1;entry=self.entry();self.image=read_image(entry['path']);self.objects=[];self.history=[];self.redo=[]
        for field in (self.group,self.split,self.reviewed):field.blockSignals(True)
        self.group.setText(entry['group']);self.split.setCurrentText(entry['split']);self.reviewed.setChecked(entry.get('reviewed',False))
        for field in (self.group,self.split,self.reviewed):field.blockSignals(False)
        self.canvas.set_image(self.image);self.refresh()

    def save_project(self,force_dialog=False):
        self.require_image();self.sync_entry();self.sync_profiles();validate_project(self.project)
        file=getattr(self,'project_file',None)
        if force_dialog or not file:file,_=QFileDialog.getSaveFileName(self,'儲存人工標註',file or '','JSON (*.json)')
        if file:atomic_json(file,self.project);self.mark_saved(file)

    def open_project(self):
        self.idle();file,_=QFileDialog.getOpenFileName(self,'開啟人工標註','','JSON (*.json)')
        if not file:return
        project=json.loads(Path(file).read_text(encoding='utf-8-sig'));validate_project(project)
        self.project=project;self.current=-1;self.profile_table();self.reload_images();self.mark_saved(file);self.go_step(1)

    def add_class(self):
        self.idle();name,ok=QInputDialog.getText(self,'新增類別','穩定英文識別名稱（例如 resistor_0402、screw_m3）：')
        name=name.strip()
        if ok and name and name not in self.project['classes']:
            self.sync_profiles();self.project['classes'].append(name);self.profile_table()

    def remember(self):
        if self.current<0:return
        self.history.append((copy.deepcopy(self.entry()),copy.deepcopy(self.objects),copy.deepcopy(self.project['profiles'])))
        self.history=self.history[-64:];self.redo=[]

    def restore(self,source,target):
        self.require_image()
        if not source:return
        target.append((copy.deepcopy(self.entry()),copy.deepcopy(self.objects),copy.deepcopy(self.project['profiles'])))
        entry,self.objects,self.project['profiles']=source.pop();self.project['images'][self.current]=entry
        self.profile_table();self.reviewed.setChecked(entry.get('reviewed',False));self.refresh()

    def undo(self):self.restore(self.history,self.redo)
    def redo_action(self):self.restore(self.redo,self.history)

    def invalidate_review(self):
        self.entry()['reviewed']=False;self.reviewed.setChecked(False)

    def drawn(self,points,is_roi):
        self.require_image();h,w=self.image.shape[:2];p=polygon(points)
        if not inside(p,(0,0,w,h)):raise ValueError('選區／標註超出影像')
        if self.canvas.mode.startswith('inspection-'):return self.inspection_drawn(points)
        if is_roi:
            xs=[v[0] for v in points];ys=[v[1] for v in points]
            return self.infer([int(min(xs)),int(min(ys)),max(1,int(max(xs)-min(xs))),max(1,int(max(ys)-min(ys)))])
        if self.canvas.mode=='cad-rect':
            from .model import pose_from_polygon
            self.sync_profiles();name=(self.kind.currentData() or self.kind.currentText());validate_profiles(self.project['profiles'],[name])
            pose={**pose_from_polygon(points),**{k:self.project['profiles'][name][k] for k in ('w','h')}}
            if not inside(corners(pose),(0,0,w,h)):raise ValueError('指定 CAD 尺寸超出影像')
            if not fits_inspection(pose,self.project['profiles'][name],(0,0,w,h)):raise ValueError('相依檢測框超出影像')
            self.remember();self.objects.append({'class':name,'pose':pose,'observed':pose.copy(),'outline':[], 'confidence':0,'manual':True})
            self.view.setCurrentIndex(1);self.refresh();return
        shape='polygon' if self.canvas.mode=='annotation-poly' or not is_rectangle(points) else 'rectangle'
        from .model import pose_from_polygon
        name=self.kind.currentData() or self.kind.currentText()
        if not fits_inspection(pose_from_polygon(points),self.project['profiles'].get(name,{}),(0,0,w,h)):
            raise ValueError('相依檢測框超出影像，請調整元件位置。')
        self.remember();self.entry()['annotations'].append({'class':(self.kind.currentData() or self.kind.currentText()),'points':points,'shape':shape})
        self.invalidate_review();self.view.setCurrentIndex(0);self.refresh();self.annotation_added()

    def moved(self,index,dx,dy):
        self.require_image();h,w=self.image.shape[:2]
        if self.view.currentIndex()==0:
            obj=self.entry()['annotations'][index];points=[[x+dx,y+dy] for x,y in obj['points']]
            from .model import pose_from_polygon
            if inside(points,(0,0,w,h)) and fits_inspection(pose_from_polygon(points),self.project['profiles'].get(obj['class'],{}),(0,0,w,h)):
                obj['points']=points;self.invalidate_review()
        else:
            obj=self.objects[index];pose={**obj['pose'],'x':obj['pose']['x']+dx,'y':obj['pose']['y']+dy}
            if inside(corners(pose),(0,0,w,h)) and fits_inspection(pose,self.project['profiles'].get(obj['class'],{}),(0,0,w,h)):
                obj.update(pose=pose,manual=True)
        self.refresh()

    def delete(self):
        self.require_image();objects=self.objects if self.view.currentIndex()==1 else self.entry()['annotations']
        if self.canvas.points or self.canvas.start is not None:
            self.show_notice('請先按 Enter 完成輪廓，或按 Esc 取消繪製，再刪除選取框。');return
        if not 0<=self.selected_index<len(objects):
            self.show_notice('請先點選要刪除的框，再按 Delete。');return
        index=self.selected_index;self.remember();objects.pop(index)
        if self.view.currentIndex()==0:self.invalidate_review()
        self.selected_index=-1;self.refresh()
        self.show_notice(f'已刪除 #{index+1}。按 Ctrl+Z 可復原。')

    def vertex_moved(self,index,vertex,x,y):
        self.require_image();label=self.entry()['annotations'][index];shape=annotation_shape(label)
        points=resized_vertex(label,vertex,x,y)
        h,w=self.image.shape[:2]
        try:
            polygon(points)
            if not inside(points,(0,0,w,h)):raise ValueError('頂點超出影像')
            from .model import pose_from_polygon
            if not fits_inspection(pose_from_polygon(points),self.project['profiles'].get(label['class'],{}),(0,0,w,h)):
                raise ValueError('相依檢測框超出影像')
            label['points']=points;label['shape']=shape;self.invalidate_review()
        finally:self.refresh()

    def apply_geometry(self):
        self.require_image()
        if self.view.currentIndex()!=1 or not 0<=self.selected_index<len(self.objects):raise ValueError('請選取一個 CAD')
        pose={k:v.value() for k,v in self.fields.items()};h,w=self.image.shape[:2]
        if min(pose['w'],pose['h'])<=0 or not inside(corners(pose),(0,0,w,h)):raise ValueError('CAD 尺寸無效或超出影像')
        obj=self.objects[self.selected_index]
        if not fits_inspection(pose,self.project['profiles'][obj['class']],(0,0,w,h)):raise ValueError('相依檢測框超出影像')
        self.remember();self.objects[self.selected_index].update(pose=pose,manual=True);self.refresh()

    def export_data(self):
        from .dataset import export_dataset
        self.require_image();self.sync_entry();self.sync_profiles()
        directory=QFileDialog.getExistingDirectory(self,'選擇空目錄：匯出 OBB + segmentation')
        if directory:self.dataset_path=str(export_dataset(self.project,directory));self.log.appendPlainText('已匯出 '+self.dataset_path);self.show_notice('資料已匯出，可按「開始訓練」繼續。');self.update_guidance()

    def load_model(self,file=None):
        self.idle()
        if file is None:file,_=QFileDialog.getOpenFileName(self,'載入訓練完成的 model.json','','JSON (*.json)')
        if not file:return
        spec=json.loads(Path(file).read_text(encoding='utf-8-sig'))
        if spec.get('format')!='pcb-dl-model' or spec.get('task') not in ('obb','segment'):raise ValueError('無效模型 manifest')
        validate_profiles(spec['profiles'],spec['classes'])
        self.sync_profiles()
        profiles={**copy.deepcopy(spec['profiles']),**copy.deepcopy(self.project['profiles'])}
        if self.objects and self.project['profiles']!=profiles:
            raise ValueError('新模型 CAD 預設規格與目前配方不同，請先儲存並另開影像')
        self.manifest=file;self.manifest_task=spec['task'];self.task.setCurrentText(spec['task'])
        self.project['profiles']=profiles
        for name in spec['classes']:
            if name not in self.project['classes']:self.project['classes'].append(name)
        self.profile_table()
        self.model_label.setText(('合成示範模型，不能代表真實資料品質。\n' if spec.get('demo_only') else '')+Path(file).name+' / '+spec['task']+' / '+', '.join(spec['classes']))
        self.go_step(4)

    def infer_whole(self):
        self.require_image();h,w=self.image.shape[:2];self.infer([0,0,w,h])

    def infer(self,roi):
        from .workers import InferenceWorker
        self.require_image()
        if not self.manifest:raise ValueError('請先載入已訓練模型的 model.json')
        spec=json.loads(Path(self.manifest).read_text(encoding='utf-8-sig'))
        if spec['task']!=self.task.currentText():raise ValueError('選取模式與模型 task 不符，請載入對應模型')
        self.sync_profiles()
        profiles=self.project['profiles'] or spec['profiles']
        validate_profiles(profiles,spec['classes'])
        self.worker=InferenceWorker(self.manifest,self.image.copy(),roi,self.confidence.value(),self.tile.value(),self,profiles=copy.deepcopy(profiles))
        self.worker.completed.connect(lambda objects,metadata:self.apply_inference(objects,metadata,roi))
        self.inference_error='';self.worker.failed.connect(self.inference_failed)
        self.worker.progress.connect(self.inference_progress)
        self.worker.finished.connect(self.inference_finished);self.canvas.setEnabled(False);self.worker.start();self.update_guidance();self.show_notice('正在辨識圖片，請稍候。')

    def apply_inference(self,objects,metadata,roi):
        from .inference import iou
        self.remember();kept=[o for o in self.objects if o.get('manual') or not inside(corners(o['pose']),roi)]
        for obj in objects:
            if not any(k.get('manual') and iou({'observed':obj['pose']},{'observed':k['pose']})>.3 for k in kept):kept.append(obj)
        self.objects=kept;self.provenance=metadata;self.project['profiles']={**self.project['profiles'],**copy.deepcopy(metadata['profiles'])}
        for name in metadata['classes']:
            if name not in self.project['classes']:self.project['classes'].append(name)
        self.profile_table();self.view.setCurrentIndex(1);self.canvas.set_mode('select');self.refresh()
        self.log.appendPlainText(f"完成 {metadata['task']}：{len(objects)} 候選，配方目前 {len(self.objects)} 個 CAD。")

    def save_recipe(self,force_dialog=False):
        self.require_image();h,w=self.image.shape[:2]
        recipe=make_recipe(self.entry()['path'],w,h,self.project['profiles'],self.objects,self.provenance)
        file=getattr(self,'recipe_file',None)
        if force_dialog or not file:file,_=QFileDialog.getSaveFileName(self,'儲存 元件深度學習 CAD 配方',file or '','JSON (*.json)')
        if file:atomic_json(file,recipe);self.mark_saved(file,True)

    def open_recipe(self):
        self.idle();file,_=QFileDialog.getOpenFileName(self,'開啟 元件深度學習 CAD 配方','','JSON (*.json)')
        if not file:return
        recipe=json.loads(Path(file).read_text(encoding='utf-8-sig'))
        if recipe.get('format')!='pcb-dl-cad' or recipe.get('version')!=1:raise ValueError('不是 元件深度學習 CAD 配方')
        image=read_image(recipe['image']);h,w=image.shape[:2]
        if (w,h)!=(recipe['width'],recipe['height']):raise ValueError('配方影像尺寸不一致')
        make_recipe(recipe['image'],w,h,recipe['profiles'],recipe['objects'],recipe.get('provenance',{}))
        self.project=new_project();self.project['profiles']=recipe['profiles'];self.project['classes']=list(recipe['profiles'])
        self.project['images']=[dict(path=recipe['image'],width=w,height=h,group=Path(recipe['image']).stem,split='train',reviewed=False,annotations=[])]
        self.current=-1;self.profile_table();self.reload_images();self.objects=recipe['objects'];self.provenance=recipe.get('provenance',{})
        self.view.setCurrentIndex(1);self.refresh();self.mark_saved(file,True);self.saved_project=self.signature()

    def train(self):
        self.idle()
        if self.process is not None and self.process.state()!=QProcess.NotRunning:raise ValueError('訓練仍在進行')
        dataset=getattr(self,'dataset_path','') or QFileDialog.getExistingDirectory(self,'選擇已匯出的訓練資料')
        if not dataset:return
        output=QFileDialog.getExistingDirectory(self,'選擇訓練輸出目錄（每個 task 子目錄不得已存在）')
        if not output:return
        self.process=QProcess(self);self.process.setWorkingDirectory(str(Path(__file__).resolve().parents[2]))
        self.process.setProcessChannelMode(QProcess.MergedChannels)
        self.process.readyReadStandardOutput.connect(lambda:self.log.appendPlainText(bytes(self.process.readAllStandardOutput()).decode('utf-8',errors='replace')))
        self.process.finished.connect(lambda code,status:self.training_finished(code,output))
        self.process.start(sys.executable,['-u','-m','tools.pcb_dl.cli','train',dataset,self.task.currentText(),output,'--epochs',str(self.epochs.value()),'--imgsz',str(self.imgsz.value())])
        self.training_status.setText('正在訓練；詳細進度可展開工作紀錄。');self.update_guidance()

    def cancel(self):
        if self.worker is not None:self.worker.cancelled.set()
        if self.process is not None and self.process.state()!=QProcess.NotRunning:self.process.kill()
