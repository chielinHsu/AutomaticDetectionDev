"""Teach package frames on a selected component; one transaction per change."""
import copy

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QDoubleSpinBox,QFormLayout,QLabel,QPushButton,
    QTreeWidget,QTreeWidgetItem)

from .inspection import (MAX_BOXES,fits_inspection,inspection_boxes,preset,relative,validate_inspection)
from .model import pose_from_polygon


def build_inspection_editor(w,layout):
    w.inspection_status=QLabel();w.inspection_status.setWordWrap(True);layout.addWidget(w.inspection_status)
    w.inspection_controls=[]
    def button(title,callback):
        b=QPushButton(title);b.clicked.connect(w.safe(callback));layout.addWidget(b);w.inspection_controls.append(b);return b
    button('建立雙排八腳起始框（本體＋8 導腳）',lambda:w.inspection_preset(8))
    button('建立雙端起始框（本體＋2 端點）',lambda:w.inspection_preset(2))
    button('在圖上畫／重畫中央本體',lambda:w.begin_inspection('body'))
    button('在圖上增加一個導腳框',lambda:w.begin_inspection('pin'))
    button('在圖上增加相依檢測窗',lambda:w.begin_inspection('window'))
    note=QLabel('先點選參考元件。紅框為檢測區，外框只供辨識定位。\n導腳依附本體；檢測窗依附清單選取框。修改會同步同封裝。不同封裝請新增類別。')
    note.setWordWrap(True);layout.addWidget(note)
    w.inspection_tree=QTreeWidget();w.inspection_tree.setHeaderLabels(['檢測框','父框']);w.inspection_tree.setMinimumHeight(145)
    w.inspection_tree.setMaximumHeight(230);layout.addWidget(w.inspection_tree)
    w.inspection_tree.currentItemChanged.connect(lambda *_:w.inspection_selection())
    form=QFormLayout();layout.addLayout(form);w.inspection_fields={}
    for key,title in [('x','相對父框 X (px)'),('y','相對父框 Y (px)'),('w','寬 W (px)'),('h','高 H (px)'),('angle','相對角度 (°)')]:
        field=QDoubleSpinBox();field.setRange(-1000000,1000000);field.setDecimals(3);field.setKeyboardTracking(False)
        w.inspection_fields[key]=field;form.addRow(title,field)
    button('套用選取檢測框（子框一起跟隨）',w.apply_inspection_geometry)
    button('刪除選取檢測框及其子框',w.delete_inspection)
    button('清除整組，返回舊版子窗',w.clear_inspection)
    w.inspection_key=None;w.inspection_target=None


class InspectionEditor:
    def inspection_reference(self):
        self.require_image()
        objects=self.objects if self.view.currentIndex()==1 else self.entry()['annotations']
        if not 0<=self.selected_index<len(objects):raise ValueError('請先在圖片或元件清單點選一個參考元件，再設定檢測框。')
        obj=objects[self.selected_index]
        pose=obj['pose'] if self.view.currentIndex()==1 else pose_from_polygon(obj['points'])
        return obj['class'],pose

    def update_inspection_editor(self):
        if not hasattr(self,'inspection_tree'):return
        valid=self.image is not None and self.view.currentIndex()==0 and 0<=self.selected_index<len(self.entry().get('annotations',[]))
        busy=self.worker is not None and self.worker.isRunning()
        pending=bool(self.canvas.points or self.canvas.start is not None)
        for control in self.inspection_controls:control.setEnabled(valid and not busy and not pending)
        if not valid:
            key=None;group=None
            self.inspection_status.setText('點選一個已標註元件，教導它的中央本體與導腳檢測框。')
        else:
            label=self.entry()['annotations'][self.selected_index];group=self.project['profiles'].get(label['class'],{}).get('inspection')
            key=(self.current,self.selected_index,label['class'],repr(group))
            count=len(group['boxes']) if group else 0
            self.inspection_status.setText(f'參考 #{self.selected_index+1} · {label["class"]}：{count} 個相依檢測框。'+
                ('可畫本體／導腳，或選下方檢測框改局部座標。' if count else '可先建立版型再對照影像調整，或逐一畫框。'))
        if key==self.inspection_key:return
        self.inspection_key=key
        selected=self.inspection_id();self.inspection_tree.blockSignals(True);self.inspection_tree.clear()
        if group:
            items={}
            for box in group['boxes']:
                title={'body':'本體','pin':'導腳','window':'檢測窗'}[box['kind']]+' · '+box['id']
                item=QTreeWidgetItem([title,box['parent']]);item.setData(0,Qt.UserRole,box['id']);items[box['id']]=item
            for box in group['boxes']:
                item=items[box['id']]
                if box['parent']=='component':self.inspection_tree.addTopLevelItem(item)
                else:items[box['parent']].addChild(item)
            self.inspection_tree.expandAll();self.inspection_tree.setCurrentItem(items.get(selected,items['body']))
        self.inspection_tree.blockSignals(False);self.inspection_selection()

    def inspection_id(self):
        item=self.inspection_tree.currentItem()
        return item.data(0,Qt.UserRole) if item else None

    def inspection_selection(self):
        ident=self.inspection_id();box=None
        if ident and self.current>=0 and self.selected_index>=0:
            labels=self.entry()['annotations']
            if self.selected_index<len(labels):
                group=self.project['profiles'].get(labels[self.selected_index]['class'],{}).get('inspection',{})
                box=next((b for b in group.get('boxes',[]) if b['id']==ident),None)
        for key,field in self.inspection_fields.items():
            field.setEnabled(box is not None)
            if box:field.setValue(box['pose'][key])

    def commit_inspection(self,name,profile):
        if profile.get('inspection') is not None:validate_inspection(profile['inspection'])
        # Reject the complete change before creating history or mutating any image/profile.
        for entry in self.project['images']:
            for label in entry['annotations']:
                if label['class']==name and not fits_inspection(pose_from_polygon(label['points']),profile,(0,0,entry['width'],entry['height'])):
                    raise ValueError('相依檢測框超出影像；請調整本體、導腳或檢測窗，原設定保留。')
        if self.image is not None:
            h,w=self.image.shape[:2]
            for obj in self.objects:
                if obj['class']==name and not fits_inspection(obj['pose'],profile,(0,0,w,h)):
                    raise ValueError('相依檢測框超出目前 CAD 影像，原設定保留。')
        self.remember();self.project['profiles'][name]=copy.deepcopy(profile);self.profile_table();self.refresh()
        self.show_notice('已更新同封裝的相依檢測框；本體移動／旋轉時子框一起跟隨。Ctrl+Z 可復原。')

    def inspection_preset(self,pins):
        name,pose=self.inspection_reference();self.sync_profiles()
        profile=copy.deepcopy(self.project['profiles'].get(name,dict(w=pose['w'],h=pose['h'],window_ratio=.6)))
        profile['inspection']=preset(profile['w'],profile['h'],pins)
        self.commit_inspection(name,profile)
        self.show_notice('已建立起始版型，請對照實際影像逐框調整；導腳編號僅供編輯，不代表實體 Pin 1。')

    def begin_inspection(self,kind):
        name,pose=self.inspection_reference()
        if self.step!=2:raise ValueError('請在第 3 步設定檢測框。')
        group=self.project['profiles'].get(name,{}).get('inspection')
        if kind!='body' and group is None:raise ValueError('請先畫中央本體框，或建立起始版型。')
        parent='component' if kind=='body' else 'body' if kind=='pin' else self.inspection_id() or 'body'
        self.inspection_target=(self.current,self.selected_index,name,copy.deepcopy(pose),parent)
        self.choose_tool('inspection-'+kind)
        self.show_notice({'body':'在圖片上拖出中央本體檢測區。','pin':'在圖片上拖出一隻導腳的檢測區，可重複增加。',
                          'window':'在圖片上拖出檢測窗；父框為 '+parent+'。'}[kind])

    def inspection_drawn(self,points):
        name,pose=self.inspection_reference()
        target=self.inspection_target
        if target is None or target[:3]!=(self.current,self.selected_index,name):raise ValueError('參考元件已改變，請重新選擇教框工具。')
        kind=self.canvas.mode.removeprefix('inspection-');parent=target[4]
        self.sync_profiles();profile=copy.deepcopy(self.project['profiles'].get(name,dict(w=pose['w'],h=pose['h'],window_ratio=.6)))
        group=profile.setdefault('inspection',dict(version=1,boxes=[]));boxes=group['boxes']
        parent_pose=pose if parent=='component' else next(b['pose'] for b in inspection_boxes(pose,profile) if b['id']==parent)
        world=pose_from_polygon(points);local=relative(parent_pose,world)
        if kind=='body':
            boxes[:]=[b for b in boxes if b['id']!='body'];ident='body'
        else:
            if len(boxes)>=MAX_BOXES:raise ValueError(f'每個封裝最多 {MAX_BOXES} 個檢測框')
            number=1
            while any(b['id']==f'{kind}-{number}' for b in boxes):number+=1
            ident=f'{kind}-{number}'
        boxes.append(dict(id=ident,parent=parent,kind=kind,pose=local))
        self.commit_inspection(name,profile);self.canvas.set_mode('select')

    def apply_inspection_geometry(self):
        name,_=self.inspection_reference();ident=self.inspection_id()
        if ident is None:raise ValueError('請選取檢測框清單中的本體、導腳或檢測窗。')
        profile=copy.deepcopy(self.project['profiles'][name])
        box=next(b for b in profile['inspection']['boxes'] if b['id']==ident)
        box['pose']={key:field.value() for key,field in self.inspection_fields.items()}
        self.commit_inspection(name,profile)

    def delete_inspection(self):
        name,_=self.inspection_reference();ident=self.inspection_id()
        if ident is None:raise ValueError('請先選取檢測框。')
        profile=copy.deepcopy(self.project['profiles'][name])
        if ident=='body':profile.pop('inspection')
        else:
            boxes=profile['inspection']['boxes'];removed={ident}
            for _ in boxes:
                removed.update(b['id'] for b in boxes if b['parent'] in removed)
            profile['inspection']['boxes']=[b for b in boxes if b['id'] not in removed]
        self.commit_inspection(name,profile)

    def clear_inspection(self):
        name,_=self.inspection_reference();profile=copy.deepcopy(self.project['profiles'].get(name,{}))
        if 'inspection' not in profile:return
        profile.pop('inspection');self.commit_inspection(name,profile)
