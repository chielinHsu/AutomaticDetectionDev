"""Collapsible class browser; queries change only visibility, never annotation data."""
from PySide6.QtCore import QRectF,Qt,Signal
from PySide6.QtWidgets import QHeaderView,QTreeWidget,QTreeWidgetItem
from .annotation_geometry import annotation_shape
from .canvas_items import class_icon,class_name
from .model import corners


class ObjectBrowser(QTreeWidget):
    summaryChanged=Signal(str)
    queryResetRequested=Signal()
    objectActivated=Signal(int)
    visibleObjectsChanged=Signal(object)

    def __init__(self):
        super().__init__();self.setColumnCount(3);self.setHeaderLabels(['元件／編號','形狀','來源'])
        self.header().setSectionResizeMode(0,QHeaderView.Stretch)
        self.setColumnWidth(1,62);self.setColumnWidth(2,65);self.setMinimumWidth(215)
        self.setRootIsDecorated(True);self.setAlternatingRowColors(True)
        self.query='';self.expanded_classes=set();self.rows={};self.groups={};self.search_text={};self._building=False
        self.itemExpanded.connect(lambda item:self.remember_expansion(item,True))
        self.itemCollapsed.connect(lambda item:self.remember_expansion(item,False))
        self.itemDoubleClicked.connect(self.activate_object)

    def keyPressEvent(self,event):
        if event.key() in (Qt.Key_Return,Qt.Key_Enter) and self.currentItem() is not None:
            item=self.currentItem()
            if item.parent() is None:item.setExpanded(not item.isExpanded())
            else:self.activate_object(item,0)
            event.accept();return
        super().keyPressEvent(event)

    def remember_expansion(self,item,expanded):
        if self._building or self.query or item.parent() is not None:return
        key=item.data(0,Qt.UserRole)
        if expanded:self.expanded_classes.add(key)
        else:self.expanded_classes.discard(key)

    def activate_object(self,item,column):
        index=item.data(0,Qt.UserRole)
        if isinstance(index,int):self.objectActivated.emit(index)

    def populate(self,objects,classes,cad=False,selected=-1):
        self.setColumnHidden(2,not cad)
        self._building=True;blocked=self.blockSignals(True);self.clear();self.rows={};self.groups={};self.search_text={}
        names=list(dict.fromkeys([*classes,*[o['class'] for o in objects]]))
        for name in names:
            group=QTreeWidgetItem([class_name(name),'','']);group.setData(0,Qt.UserRole,name);group.setToolTip(0,'點箭頭或雙擊類別名稱，可展開／收合')
            group.setIcon(0,class_icon(name));self.addTopLevelItem(group);self.groups[name]=group
        for index,obj in enumerate(objects):
            shape='CAD' if cad else ('矩形' if annotation_shape(obj)=='rectangle' else '輪廓')
            source='人工' if obj.get('manual',not cad) else f"{obj['confidence']:.0%}"
            name=obj.get('name','');caption=f'#{index+1}'+(' · '+name if name else '')
            item=QTreeWidgetItem([caption,shape,source]);item.setData(0,Qt.UserRole,index)
            item.setIcon(0,class_icon(obj['class']));item.setToolTip(0,f'{caption} · {class_name(obj["class"])}；雙擊放大定位')
            self.groups[obj['class']].addChild(item);self.rows[index]=item
            self.search_text[index]=f"{class_name(obj['class'])} {obj['class']} {name} #{index+1}".casefold()
        self.filter_objects(self.query,notify=False)
        if selected in self.rows:self.setCurrentItem(self.rows[selected])
        self.blockSignals(blocked);self._building=False;self.emit_summary()

    def filter_objects(self,query,notify=True):
        self.query=query.strip().casefold();blocked=self.blockSignals(True)
        for index,item in self.rows.items():
            terms=self.query.split();match=all((term==f'#{index+1}' if term.startswith('#') else term in self.search_text[index]) for term in terms)
            item.setHidden(not match)
        for name,group in self.groups.items():
            total=group.childCount();shown=sum(not group.child(i).isHidden() for i in range(total))
            group.setText(0,f'{class_name(name)}（{shown}/{total}）' if self.query else f'{class_name(name)}（{total}）')
            group.setHidden(bool(self.query) and shown==0)
            group.setExpanded(bool(self.query and shown) or (not self.query and name in self.expanded_classes))
        selected_hidden=self.currentItem() is not None and self.currentItem().isHidden()
        if selected_hidden:self.clearSelection();self.setCurrentItem(None)
        self.blockSignals(blocked)
        if notify:
            self.emit_summary()
            if selected_hidden:self.itemSelectionChanged.emit()

    def emit_summary(self):
        count=sum(not item.isHidden() for item in self.rows.values())
        self.summaryChanged.emit(f'顯示 {count} / {len(self.rows)} 個元件'+('；沒有符合項目' if self.query and count==0 else ''))
        self.visibleObjectsChanged.emit([index for index,item in self.rows.items() if not item.isHidden()] if self.query else None)

    def ensure_filter(self,index):
        item=self.rows.get(index)
        if item is not None and (item.isHidden() or item.parent().isHidden()):self.queryResetRequested.emit()

    def selectRow(self,index):
        item=self.rows.get(index)
        if item is not None:
            item.parent().setExpanded(True);self.setCurrentItem(item);self.scrollToItem(item)

    def currentRow(self):
        value=self.currentItem().data(0,Qt.UserRole) if self.currentItem() is not None else None
        return value if isinstance(value,int) else -1

    def setCurrentCell(self,row,column):
        if row<0:self.setCurrentItem(None)
        else:self.selectRow(row)

    def rowCount(self):return len(self.rows)


class ObjectBrowserActions:
    def update_browser_selection(self):
        objects=self.objects if self.view.currentIndex()==1 else self.entry().get('annotations',[])
        valid=0<=self.selected_index<len(objects)
        self.name_panel.setVisible(valid)
        if valid:
            obj=objects[self.selected_index];self.name_title.setText(f'#{self.selected_index+1} · {class_name(obj["class"])}')
            if not self.object_name.hasFocus():self.object_name.setText(obj.get('name',''))

    def rename_selected(self):
        self.require_image();objects=self.objects if self.view.currentIndex()==1 else self.entry()['annotations'];index=self.selected_index
        if not 0<=index<len(objects):return
        name=self.object_name.text().strip()
        if len(name)>128:raise ValueError('名稱最多 128 個字元')
        if objects[index].get('name','')==name:return
        self.remember()
        if name:objects[index]['name']=name
        else:objects[index].pop('name',None)
        self.refresh();self.show_notice('已更新名稱，可在左側搜尋。名稱由你確認，不會自動猜測 元件料號。')

    def focus_object(self,index):
        self.select(index);self.canvas.choose(index)
        objects=self.objects if self.view.currentIndex()==1 else self.entry()['annotations']
        if not 0<=index<len(objects):return
        obj=objects[index];points=corners(obj['pose']) if self.view.currentIndex()==1 else obj['points']
        xs=[p[0] for p in points];ys=[p[1] for p in points];x=(min(xs)+max(xs))/2;y=(min(ys)+max(ys))/2
        extent=max(max(xs)-min(xs),max(ys)-min(ys),42)*3
        self.canvas.fitInView(QRectF(x-extent/2,y-extent/2,extent,extent),Qt.KeepAspectRatio)
        self.show_notice(f'已放大定位 #{index+1} · {obj.get("name",class_name(obj["class"]))}。按「適配視窗」可回到全圖。')

    def toggle_object_list(self):
        self.list_visible=not self.list_visible;self.update_guidance()
