"""Synchronize the shell with the existing workflow, without owning document history."""
import math
import copy
from collections import Counter
from pathlib import Path
from PySide6.QtCore import QRect,QSize,Qt,Slot
from PySide6.QtGui import QIcon,QImage,QPixmap
from PySide6.QtWidgets import QListWidgetItem
from .annotation_geometry import annotation_shape
from .canvas_items import class_icon,class_name
from .model import corners,inside,polygon,pose_from_polygon
from .workbench_theme import apply_theme
from .thumbnail_loader import request_thumbnail
from .inspection import fits_inspection,inspection_boxes


class WorkbenchPresentation:
    @Slot(str,QImage)
    def thumbnail_ready(self,path,image):
        self._thumbnail_jobs.pop(path,None)
        if self._thumbnail_cancel.is_set() or path not in self._image_paths:return
        icon=QIcon(QPixmap.fromImage(image));self._thumbnail_cache[path]=icon
        self.image_list.item(self._image_paths.index(path)).setIcon(icon)

    def change_theme(self,mode):apply_theme(self,mode,persist=True)

    def select_image_row(self,index):
        if self._ready and index>=0 and index!=self.current:self.images.setCurrentIndex(index)

    def change_image(self,delta):
        index=self.current+delta
        if 0<=index<len(self.project['images']):self.images.setCurrentIndex(index)

    def update_workbench(self):
        entries=self.project['images'];paths=tuple(e['path'] for e in entries)
        self.image_list.blockSignals(True)
        if getattr(self,'_image_paths',None)!=paths:
            self._image_paths=paths;self.image_list.clear()
            self._thumbnail_cache={p:icon for p,icon in self._thumbnail_cache.items() if p in paths}
            for entry in entries:
                path=entry['path']
                item=QListWidgetItem(self._thumbnail_cache.get(path,QIcon()),'');item.setSizeHint(QSize(200,64));item.setToolTip(path);self.image_list.addItem(item)
        if self.image is not None and self.current>=0:
            current_path=self.entry()['path']
            if current_path not in self._thumbnail_cache:
                self._thumbnail_cache[current_path]=QIcon(self.canvas.pixmap.scaled(72,50,Qt.KeepAspectRatio,Qt.SmoothTransformation))
                self.image_list.item(self.current).setIcon(self._thumbnail_cache[current_path])
            for path in paths:
                if path not in self._thumbnail_cache and path not in self._thumbnail_jobs and not self._thumbnail_cancel.is_set():request_thumbnail(self,path)
        for i,entry in enumerate(entries):
            self.image_list.item(i).setText(f'{Path(entry["path"]).name}\n{entry["width"]} × {entry["height"]}  ·  {len(entry["annotations"])} 標註')
        self.image_list.setCurrentRow(self.current);self.image_list.blockSignals(False)
        if self.current>=0:self.image_list.scrollToItem(self.image_list.item(self.current))
        self.images.blockSignals(True);self.images.setCurrentIndex(self.current);self.images.blockSignals(False)
        self.image_title.setText(Path(self.entry()['path']).name if self.current>=0 else '尚未載圖')
        self.image_previous.setEnabled(self.current>0);self.image_next.setEnabled(0<=self.current<len(entries)-1)
        objects=self.objects if self.view.currentIndex()==1 else self.entry().get('annotations',[])
        counts=Counter(o['class'] for o in objects)
        self.class_counts.setText('目前圖片 · '+ ' / '.join(f'{class_name(k)} {counts[k]}' for k in self.project['classes']))
        for i in range(self.kind.count()):self.kind.setItemIcon(i,class_icon(self.kind.itemData(i)))
        valid=0<=self.selected_index<len(objects)
        busy=self.worker is not None and self.worker.isRunning()
        self.delete_button.setEnabled(valid and not busy)
        self.update_annotation_inspector(objects,valid)
        self.update_canvas_status()

    def update_canvas_status(self):
        if not self._ready:return
        zoom=self.canvas.transform().m11()*100
        self.zoom_choice.setCurrentIndex(-1);self.zoom_choice.setPlaceholderText(f'{zoom:.0f}%')
        names={'select':'選取','pan':'平移','annotation-rect':'矩形','annotation-poly':'多邊形','roi':'辨識選區','cad-rect':'手動 CAD'}
        entry=self.entry();count=len(self.objects) if self.view.currentIndex()==1 else len(entry.get('annotations',[]))
        self.canvas_status.setText(f'{count} 個物件  ·  {names.get(self.canvas.mode,self.canvas.mode)}  ·  '+
            (f'{entry["width"]} × {entry["height"]} px  ·  {zoom:.0f}%' if entry else '尚未載入影像'))

    def update_annotation_inspector(self,objects,valid):
        annotation=valid and self.view.currentIndex()==0
        self.annotation_details.setVisible(annotation)
        self.reviewed.setEnabled(self.image is not None and not self.canvas.points and self.canvas.start is None)
        if not annotation:
            self.crop_preview.clear();self.geometry_status.clear();self._crop_key=None;self._inspected_object=None;return
        obj=objects[self.selected_index]
        if tuple(self.selected_class.itemData(i) for i in range(self.selected_class.count()))!=tuple(self.project['classes']):
            self.selected_class.clear()
            for name in self.project['classes']:self.selected_class.addItem(class_name(name)+' ('+name+')',name)
        # Do not overwrite a category or numeric edit while the user is typing.
        changed=getattr(self,'_inspected_object',None)!=self.inspected_key(obj)
        if changed:
            self.selected_class.setCurrentIndex(self.selected_class.findData(obj['class']))
            pose=pose_from_polygon(obj['points'])
            # Writing the fields from the document must not come back as a user edit.
            for key,field in self.annotation_fields.items():
                field.blockSignals(True);field.setValue(pose[key]);field.blockSignals(False)
            self._field_error=''
        self._inspected_object=self.inspected_key(obj)
        rectangle=annotation_shape(obj)=='rectangle'
        for field in self.annotation_fields.values():field.setEnabled(rectangle)
        busy=self.worker is not None and self.worker.isRunning()
        self.annotation_apply.setEnabled(rectangle and not busy)
        points=obj['points'];height,width=self.image.shape[:2]
        try:polygon(points);ok=inside(points,(0,0,width,height))
        except ValueError:ok=False
        self.geometry_status.setText('欄位的值尚未套用\n框維持上一個有效狀態' if self._field_error else '幾何有效\n類別已選擇\n在影像範圍內' if ok else '需要修正幾何範圍')
        # Crop only the selected object's image region; the full-resolution pixmap is cached by Canvas.
        x=max(0,math.floor(min(p[0] for p in points)));y=max(0,math.floor(min(p[1] for p in points)))
        right=min(width,math.ceil(max(p[0] for p in points)));bottom=min(height,math.ceil(max(p[1] for p in points)))
        crop_key=(self.current,id(self.canvas.pixmap),x,y,right,bottom)
        if getattr(self,'_crop_key',None)!=crop_key:
            self._crop_key=crop_key
            crop=self.canvas.pixmap.copy(QRect(x,y,max(1,right-x),max(1,bottom-y)))
            self.crop_preview.setPixmap(crop.scaled(self.crop_preview.size(),Qt.KeepAspectRatio,Qt.SmoothTransformation))

    def reclassify_selected(self):
        self.kind.setCurrentIndex(self.kind.findData(self.selected_class.currentData()));self.reclassify()

    def apply_annotation_geometry(self):
        self.require_image();labels=self.entry()['annotations'];index=self.selected_index
        if self.view.currentIndex()!=0 or not 0<=index<len(labels):return
        obj=labels[index]
        if annotation_shape(obj)!='rectangle':raise ValueError('多邊形請以頂點編輯；先明確轉成矩形才能套用長寬。')
        points=self.annotation_field_points(obj);self._field_error='';self._live_edit=None
        if points!=obj['points']:
            self.remember();obj['points']=points;obj['shape']='rectangle';self.invalidate_review()
        # Show the stored pose (long side as width, angle within -90..90) in the fields again.
        self._inspected_object=None;self.refresh()
        self.show_notice('已套用矩形幾何；Ctrl+Z 可復原。')

    def annotation_field_points(self,obj):
        """Corners for the numeric fields, or ValueError; the document is not touched."""
        pose={key:field.value() for key,field in self.annotation_fields.items()}
        if pose['w']<2 or pose['h']<2:raise ValueError('寬與高至少需要 2 px；原框未變更。')
        points=corners(pose);polygon(points);height,width=self.image.shape[:2]
        if not inside(points,(0,0,width,height)):raise ValueError('修改後超出影像，請調整中心、長寬或角度；原框未變更。')
        # The same normalized pose the canvas, dataset and recipe derive from the stored corners.
        if not fits_inspection(pose_from_polygon(points),self.project['profiles'].get(obj['class'],{}),(0,0,width,height)):
            raise ValueError('相依檢測框超出影像；原框未變更。')
        return points

    def inspected_key(self,obj):
        return (self.current,self.selected_index,id(obj),tuple(map(tuple,obj['points'])),obj['class'])

    def end_geometry_adjustment(self):
        # Enter, Tab or leaving the field closes the adjustment; the next change is a new undo step.
        self._live_edit=None

    def live_annotation_geometry(self,key):
        """A field value changed: move, resize or rotate the selected rectangle at once."""
        if not self._ready:return
        try:
            self.require_image();labels=self.entry()['annotations'];index=self.selected_index
            if self.view.currentIndex()!=0 or not 0<=index<len(labels):return
            obj=labels[index]
            if annotation_shape(obj)!='rectangle':return
            points=self.annotation_field_points(obj)
        except Exception as exc:
            # Keep the last valid frame; the fields keep what the user entered so it can be corrected.
            self._field_error=str(exc);self.show_notice(str(exc),True);self.update_guidance();return
        # One undo step for one continuous adjustment of one field of one frame. Any other edit or an
        # undo changes the newest history entry, which also ends the adjustment.
        burst=(self.current,index,id(obj),key);newest=self.history[-1] if self.history else None
        if self._live_edit is None or self._live_edit[0]!=burst or self._live_edit[1] is not newest:
            self.remember();self._live_edit=(burst,self.history[-1])
        obj['points']=points;obj['shape']='rectangle';self._field_error=''
        # The fields already show the user's values: do not replace them with the normalized pose
        # mid-adjustment (set before invalidate_review, whose signal re-runs the inspector).
        self._inspected_object=self.inspected_key(obj);self.invalidate_review()
        profile=self.project['profiles'].get(obj['class'],{});shown=copy.deepcopy(obj)
        shown['window_ratio']=profile.get('window_ratio',.6);shown['inspection_boxes']=inspection_boxes(pose_from_polygon(points),profile)
        if self.canvas.update_object(index,shown):self.update_guidance()
        else:self.refresh()
        self.show_notice('已即時更新矩形幾何；Ctrl+Z 可復原這次調整。')
