"""Qt canvas for polygon annotation, ROI drawing and draggable rotated CAD."""
import math
from PySide6.QtCore import QPointF,Qt,Signal
from PySide6.QtGui import QColor,QImage,QPainter,QPen,QPixmap,QPolygonF
from PySide6.QtWidgets import QGraphicsEllipseItem,QGraphicsItem,QGraphicsPolygonItem,QGraphicsScene,QGraphicsView

from .model import corners,inside,polygon
from .annotation_geometry import resized_vertex
from .canvas_items import HaloPolygon,MovablePolygon,Vertex,class_color,class_name,pixel_pen
from .draft_items import ContourDraft
from .inspection import frame_corners,compose,relative
from .model import pose_from_polygon


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
    draftChanged=Signal(int)
    draftCancelled=Signal()

    def __init__(self):
        super().__init__();self.setScene(QGraphicsScene(self));self.mode='select';self.points=[];self.start=None
        self.draft=None;self.image=None;self.cad=False;self.handles=[];self.display=[];self.pixmap=None;self.show_all_labels=False;self.visible_objects=None
        self.cursor_point=None;self.setMouseTracking(True);self.viewport().setMouseTracking(True)
        self.setRenderHint(QPainter.Antialiasing)
        # Merge simultaneous overlay/handle damage into one region, including fractional-DPI edges.
        self.setViewportUpdateMode(QGraphicsView.BoundingRectViewportUpdate)
        self.setCacheMode(QGraphicsView.CacheBackground)
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
                original=pose_from_polygon(self.display[index]['points']);changed=pose_from_polygon(points)
                for child in item.childItems():
                    if hasattr(child,'inspection_pose'):
                        pose=compose(changed,relative(original,child.inspection_pose))
                        child.setPolygon(QPolygonF([QPointF(*p) for p in frame_corners(pose)]))
        for handle in self.handles:handle.setPos(*points[handle.vertex])

    def set_mode(self,mode):
        self.cancel_draft(notify=False);self.mode=mode
        self.setInteractive(mode!='pan')
        self.setDragMode(QGraphicsView.ScrollHandDrag if mode in ('select','pan') else QGraphicsView.NoDrag)
        self.setCursor(Qt.OpenHandCursor if mode=='pan' else Qt.ArrowCursor if mode=='select' else Qt.CrossCursor)
        self.modeChanged.emit(mode)

    def set_image(self,image):
        self.image=image.copy();rgb=image[:,:,::-1].copy();h,w=rgb.shape[:2]
        qim=QImage(rgb.data,w,h,rgb.strides[0],QImage.Format_RGB888).copy()
        self.pixmap=QPixmap.fromImage(qim);self.resetCachedContent();self.render([],False,True)

    def drawBackground(self,painter,rect):
        # Keep the immutable image in the viewport-sized background cache. Resampling
        # a textured pixmap for each small overlay clip can otherwise shift pixel rows.
        brush=self.backgroundBrush()
        if brush.style()==Qt.NoBrush:brush=self.scene().backgroundBrush()
        if brush.style()==Qt.NoBrush:brush=self.palette().base()
        painter.fillRect(rect,brush)
        if self.pixmap is not None:painter.drawPixmap(QPointF(0,0),self.pixmap)

    def render(self,objects,cad,fit=False,ratio=.6):
        self.handles=[];self.display=objects;self.cad=cad;self.scene().blockSignals(True);self.scene().clear();self.scene().blockSignals(False);self.draft=None
        if self.image is None:return
        h,w=self.image.shape[:2]
        self.scene().setSceneRect(0,0,w,h)
        for index,obj in enumerate(objects):
            points=corners(obj['pose']) if cad else obj['points']
            caption=f"#{index+1}  "+(obj['name']+' · ' if obj.get('name') else '')+class_name(obj['class'])
            item=MovablePolygon(points,index,self,caption,'#52FFB8' if cad else class_color(obj['class']));self.scene().addItem(item)
            item.setVisible(self.visible_objects is None or index in self.visible_objects)
            boxes=obj.get('inspection_boxes',[])
            for box in boxes:
                region=HaloPolygon(frame_corners(box['pose']),'#FF4545',item)
                region.inspection_pose=box['pose'];region.setAcceptedMouseButtons(Qt.NoButton)
                region.setToolTip(f"{box['id']} → {box['parent']}")
            if cad:
                if not boxes:
                    window=HaloPolygon(corners(obj['pose'],obj.get('window_ratio',ratio)),'#4AE8FF',item)
                    window.setAcceptedMouseButtons(Qt.NoButton)
                if obj.get('outline') and not obj.get('manual'):
                    mask=HaloPolygon(obj['outline'],'#FF70CF',item)
                    mask.setAcceptedMouseButtons(Qt.NoButton)
        self.update_overlay_bounds()
        if self.points and self.mode=='annotation-poly':self.draw_draft(self.points)
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
        if self.mode=='annotation-poly':
            if not points:self.clear_draft_item();return
            if not isinstance(self.draft,ContourDraft):
                self.clear_draft_item();self.draft=ContourDraft(points);self.draft.set_view_scale(self.view_scale());self.scene().addItem(self.draft)
            self.draft.set_points(points)
            closing=self.cursor_point is not None and self.near_start(QPointF(*self.cursor_point)) and len(points)>=3
            self.draft.set_cursor(self.points[0] if closing else self.cursor_point,closing);return
        if self.draft is not None:self.scene().removeItem(self.draft)
        self.draft=HaloPolygon(points,'#FFCB47');self.draft.set_view_scale(self.view_scale());self.draft.setZValue(15);self.scene().addItem(self.draft)

    def clear_draft_item(self):
        if self.draft is not None:self.scene().removeItem(self.draft);self.draft=None

    def cancel_draft(self,notify=True):
        pending=bool(self.points or self.start is not None or self.draft is not None)
        self.points=[];self.start=None;self.cursor_point=None;self.clear_draft_item();self.draftChanged.emit(0)
        if pending and notify:self.draftCancelled.emit()

    def near_start(self,point):
        if not self.points:return False
        a=self.mapFromScene(QPointF(*self.points[0]));b=self.mapFromScene(point)
        return math.hypot(a.x()-b.x(),a.y()-b.y())<=8

    def finish_polygon(self):
        if self.mode!='annotation-poly':return False
        if len(self.points)<3:self.drawRejected.emit('輪廓尚未完成：至少需要三個不同的頂點。');return False
        points=[list(point) for point in self.points]
        try:
            polygon(points)
            if self.image is None:raise ValueError('請先載入影像')
            h,w=self.image.shape[:2]
            if not inside(points,(0,0,w,h)):raise ValueError('輪廓超出影像')
        except ValueError as exc:self.drawRejected.emit('輪廓尚未完成：'+str(exc));return False
        self.cancel_draft(notify=False);self.drawn.emit(points,False)
        self.set_mode('select');self.setFocus();return True

    def mousePressEvent(self,event):
        if self.mode not in ('select','pan','annotation-poly') and not self.mode.startswith('inspection-') and not self.points and self.start is None and any(isinstance(item,Vertex) for item in self.items(event.position().toPoint())):
            self.set_mode('select')
        if self.mode in ('select','pan'):return super().mousePressEvent(event)
        p=self.mapToScene(event.position().toPoint())
        if not self.sceneRect().contains(p):return
        if event.button()==Qt.LeftButton:
            if self.mode=='annotation-poly':
                if self.near_start(p):
                    self.finish_polygon();return
                if not self.points:self.scene().clearSelection()
                self.points.append([p.x(),p.y()]);self.cursor_point=None;self.draw_draft(self.points);self.draftChanged.emit(len(self.points))
            else:self.start=p;self.draftChanged.emit(0)

    def mouseMoveEvent(self,event):
        if self.start is not None:
            p=self.mapToScene(event.position().toPoint());a=self.start
            self.draw_draft([[a.x(),a.y()],[p.x(),a.y()],[p.x(),p.y()],[a.x(),p.y()]])
        elif self.mode=='annotation-poly' and self.points:
            p=self.mapToScene(event.position().toPoint());self.cursor_point=[p.x(),p.y()] if self.sceneRect().contains(p) else None;self.draw_draft(self.points)
        else:super().mouseMoveEvent(event)

    def mouseReleaseEvent(self,event):
        if self.mode in ('select','pan'):return super().mouseReleaseEvent(event)
        if self.start is not None:
            p=self.mapToScene(event.position().toPoint());a=self.start;self.start=None
            self.clear_draft_item()
            x1,x2=sorted([a.x(),p.x()]);y1,y2=sorted([a.y(),p.y()])
            first=self.mapFromScene(a);last=self.mapFromScene(p)
            if abs(first.x()-last.x())<5 or abs(first.y()-last.y())<5:
                self.drawRejected.emit('範圍太小，請用滾輪先放大元件，再按住左鍵拖出框。')
                if self.draft is not None:self.scene().removeItem(self.draft);self.draft=None
            elif x2-x1>=2 and y2-y1>=2:self.drawn.emit([[x1,y1],[x2,y1],[x2,y2],[x1,y2]],self.mode=='roi')
            self.draftChanged.emit(0)

    def keyPressEvent(self,event):
        if event.key() in (Qt.Key_Return,Qt.Key_Enter) and self.mode=='annotation-poly':
            self.finish_polygon()
        elif event.key()==Qt.Key_Escape:
            self.cancel_draft()
        elif event.key()==Qt.Key_Backspace and self.points:
            self.points.pop();self.cursor_point=None;self.draw_draft(self.points);self.draftChanged.emit(len(self.points))
        else:super().keyPressEvent(event)

    def wheelEvent(self,event):
        factor=1.2 if event.angleDelta().y()>0 else 1/1.2
        self.setTransformationAnchor(QGraphicsView.AnchorUnderMouse);self.zoom_by(factor)

    def fitInView(self,*args):
        super().fitInView(*args);self.update_overlay_bounds();self.zoomChanged.emit()

    def view_scale(self):
        transform=self.transform()
        return min(math.hypot(transform.m11(),transform.m12()),math.hypot(transform.m21(),transform.m22()))

    def update_overlay_bounds(self):
        for item in self.scene().items():
            if isinstance(item,HaloPolygon):item.set_view_scale(self.view_scale())

    def scale(self,sx,sy):
        super().scale(sx,sy);self.update_overlay_bounds();self.zoomChanged.emit()

    def fit_image(self):
        if self.image is not None:self.fitInView(self.sceneRect(),Qt.KeepAspectRatio)

    def zoom_by(self,factor):
        zoom=self.transform().m11()*factor
        if .01<=zoom<=20:self.scale(factor,factor)

    def set_zoom(self,percentage):
        self.zoom_by(percentage/100/max(self.transform().m11(),.001))
