"""Qt canvas for polygon annotation, ROI drawing and draggable rotated CAD."""
from PySide6.QtCore import QPointF,Qt,Signal
from PySide6.QtGui import QColor,QImage,QPainter,QPen,QPixmap,QPolygonF
from PySide6.QtWidgets import QGraphicsEllipseItem,QGraphicsItem,QGraphicsPolygonItem,QGraphicsScene,QGraphicsView

from .model import corners,inside,polygon
from .annotation_geometry import resized_vertex
from .canvas_items import HaloPolygon,MovablePolygon,Vertex,class_color,class_name,pixel_pen


class Canvas(QGraphicsView):
    drawn=Signal(list,bool)
    moved=Signal(int,float,float)
    beforeEdit=Signal()
    selected=Signal(int)
    vertexMoved=Signal(int,int,float,float)
    modeChanged=Signal(str)
    selectionCleared=Signal()
    drawRejected=Signal(str)
    zoomChanged=Signal()

    def __init__(self):
        super().__init__();self.setScene(QGraphicsScene(self));self.mode='select';self.points=[];self.start=None
        self.draft=None;self.image=None;self.cad=False;self.handles=[];self.display=[];self.pixmap=None;self.show_all_labels=False;self.visible_objects=None
        self.setRenderHint(QPainter.Antialiasing)
        self.setDragMode(QGraphicsView.ScrollHandDrag)
        self.scene().selectionChanged.connect(self.selection)

    def selection(self):
        items=[i for i in self.scene().selectedItems() if isinstance(i,MovablePolygon)]
        if items:
            self.selected.emit(items[0].index);self.show_handles(items[0].index)
        else:self.show_handles(-1);self.selectionCleared.emit()

    def show_handles(self,index):
        for handle in self.handles:self.scene().removeItem(handle)
        self.handles=[]
        if self.cad or not 0<=index<len(self.display) or (self.visible_objects is not None and index not in self.visible_objects):return
        for vertex,point in enumerate(self.display[index]['points']):
            handle=Vertex(self,index,vertex,point);self.scene().addItem(handle);self.handles.append(handle)

    def preview_vertex(self,index,vertex,x,y):
        if self.cad or not 0<=index<len(self.display):return
        try:
            points=resized_vertex(self.display[index],vertex,x,y);polygon(points)
            h,w=self.image.shape[:2]
            if not inside(points,(0,0,w,h)):return
        except ValueError:return
        for item in self.scene().items():
            if isinstance(item,MovablePolygon) and item.index==index:
                item.setPolygon(QPolygonF([QPointF(*p) for p in points]));item.badge.setPos(min(p[0] for p in points),min(p[1] for p in points))
        for handle in self.handles:handle.setPos(*points[handle.vertex])

    def set_mode(self,mode):
        self.mode=mode;self.points=[];self.start=None
        if self.draft is not None:self.scene().removeItem(self.draft);self.draft=None
        self.setInteractive(mode!='pan')
        self.setDragMode(QGraphicsView.ScrollHandDrag if mode in ('select','pan') else QGraphicsView.NoDrag)
        self.setCursor(Qt.OpenHandCursor if mode=='pan' else Qt.ArrowCursor if mode=='select' else Qt.CrossCursor)
        self.modeChanged.emit(mode)

    def set_image(self,image):
        self.image=image.copy();rgb=image[:,:,::-1].copy();h,w=rgb.shape[:2]
        qim=QImage(rgb.data,w,h,rgb.strides[0],QImage.Format_RGB888).copy()
        self.pixmap=QPixmap.fromImage(qim);self.render([],False,True)

    def render(self,objects,cad,fit=False,ratio=.6):
        self.handles=[];self.display=objects;self.cad=cad;self.scene().blockSignals(True);self.scene().clear();self.scene().blockSignals(False);self.draft=None
        if self.image is None:return
        h,w=self.image.shape[:2]
        self.scene().addPixmap(self.pixmap);self.scene().setSceneRect(0,0,w,h)
        for index,obj in enumerate(objects):
            points=corners(obj['pose']) if cad else obj['points']
            caption=f"#{index+1}  "+(obj['name']+' · ' if obj.get('name') else '')+class_name(obj['class'])
            item=MovablePolygon(points,index,self,caption,'#52FFB8' if cad else class_color(obj['class']));self.scene().addItem(item)
            item.setVisible(self.visible_objects is None or index in self.visible_objects)
            if cad:
                window=HaloPolygon(corners(obj['pose'],obj.get('window_ratio',ratio)),'#4AE8FF',item)
                window.setAcceptedMouseButtons(Qt.NoButton)
                if obj.get('outline') and not obj.get('manual'):
                    mask=HaloPolygon(obj['outline'],'#FF70CF',item)
                    mask.setAcceptedMouseButtons(Qt.NoButton)
        if fit:self.fitInView(self.sceneRect(),Qt.KeepAspectRatio)

    def set_label_visibility(self,show_all):
        self.show_all_labels=show_all
        for item in self.scene().items():
            if isinstance(item,MovablePolygon):item.badge.setVisible(show_all or item.isSelected())

    def set_visible_objects(self,indices):
        self.visible_objects=None if indices is None else set(indices);self.scene().blockSignals(True)
        for item in self.scene().items():
            if isinstance(item,MovablePolygon):item.setVisible(indices is None or item.index in self.visible_objects)
        self.scene().blockSignals(False)

    def choose(self,index):
        self.scene().blockSignals(True)
        for item in self.scene().items():
            if isinstance(item,MovablePolygon):item.setSelected(item.index==index)
        self.scene().blockSignals(False)
        self.show_handles(index)

    def draw_draft(self,points):
        if self.draft is not None:self.scene().removeItem(self.draft)
        self.draft=HaloPolygon(points,'#FFCB47');self.draft.setZValue(15);self.scene().addItem(self.draft)

    def mousePressEvent(self,event):
        if self.mode not in ('select','pan') and not self.points and self.start is None and any(isinstance(item,Vertex) for item in self.items(event.position().toPoint())):
            self.set_mode('select')
        if self.mode in ('select','pan'):return super().mousePressEvent(event)
        p=self.mapToScene(event.position().toPoint())
        if not self.sceneRect().contains(p):return
        if event.button()==Qt.LeftButton:
            if self.mode=='annotation-poly':
                self.points.append([p.x(),p.y()]);self.draw_draft(self.points)
            else:self.start=p

    def mouseMoveEvent(self,event):
        if self.start is not None:
            p=self.mapToScene(event.position().toPoint());a=self.start
            self.draw_draft([[a.x(),a.y()],[p.x(),a.y()],[p.x(),p.y()],[a.x(),p.y()]])
        else:super().mouseMoveEvent(event)

    def mouseReleaseEvent(self,event):
        if self.mode in ('select','pan'):return super().mouseReleaseEvent(event)
        if self.start is not None:
            p=self.mapToScene(event.position().toPoint());a=self.start;self.start=None
            x1,x2=sorted([a.x(),p.x()]);y1,y2=sorted([a.y(),p.y()])
            first=self.mapFromScene(a);last=self.mapFromScene(p)
            if abs(first.x()-last.x())<5 or abs(first.y()-last.y())<5:
                self.drawRejected.emit('範圍太小，請用滾輪先放大元件，再按住左鍵拖出框。')
                if self.draft is not None:self.scene().removeItem(self.draft);self.draft=None
            elif x2-x1>=2 and y2-y1>=2:self.drawn.emit([[x1,y1],[x2,y1],[x2,y2],[x1,y2]],self.mode=='roi')

    def keyPressEvent(self,event):
        if event.key()==Qt.Key_Return and self.mode=='annotation-poly' and len(self.points)>=3:
            points=self.points;self.points=[];self.drawn.emit(points,False)
        elif event.key()==Qt.Key_Escape:
            self.points=[];self.start=None
            if self.draft is not None:self.scene().removeItem(self.draft);self.draft=None
        elif event.key()==Qt.Key_Backspace and self.points:
            self.points.pop();self.draw_draft(self.points)
        else:super().keyPressEvent(event)

    def wheelEvent(self,event):
        factor=1.2 if event.angleDelta().y()>0 else 1/1.2
        self.setTransformationAnchor(QGraphicsView.AnchorUnderMouse);self.zoom_by(factor)

    def fitInView(self,*args):
        super().fitInView(*args);self.zoomChanged.emit()

    def fit_image(self):
        if self.image is not None:self.fitInView(self.sceneRect(),Qt.KeepAspectRatio)

    def zoom_by(self,factor):
        zoom=self.transform().m11()*factor
        if .01<=zoom<=20:self.scale(factor,factor);self.zoomChanged.emit()

    def set_zoom(self,percentage):
        self.zoom_by(percentage/100/max(self.transform().m11(),.001))
