"""Screen-pixel overlays stay legible on full-resolution PCB images."""
import hashlib
from PySide6.QtCore import QPointF,QRectF,Qt
from PySide6.QtGui import QColor,QFont,QFontMetricsF,QIcon,QPainter,QPen,QPixmap,QPolygonF
from PySide6.QtWidgets import QGraphicsItem,QGraphicsPolygonItem,QGraphicsRectItem

NAMES={'resistor':'電阻','capacitor':'電容','ic':'IC','chip':'IC','inductor':'電感','diode':'二極體','transistor':'電晶體','connector':'連接器','other':'其他'}
CLASS_COLORS={'resistor':'#3B82F6','capacitor':'#F59E0B','inductor':'#22C55E','ic':'#A855F7','chip':'#A855F7',
              'diode':'#EF4444','transistor':'#06B6D4','connector':'#FACC15','other':'#94A3B8'}
_CLASS_ICONS={}


def class_color(name):
    base=name.split('_',1)[0]
    if base in CLASS_COLORS:return QColor(CLASS_COLORS[base])
    colors=list(CLASS_COLORS.values());return QColor(colors[int.from_bytes(hashlib.sha256(name.encode()).digest()[:2],'big')%len(colors)])


def class_icon(name):
    color=class_color(name).name()
    if color not in _CLASS_ICONS:
        pixmap=QPixmap(16,16);pixmap.fill(Qt.transparent);painter=QPainter(pixmap);painter.setRenderHint(QPainter.Antialiasing)
        painter.setBrush(QColor(color));painter.setPen(QPen(QColor('#172B43'),1));painter.drawEllipse(2,2,12,12);painter.end()
        _CLASS_ICONS[color]=QIcon(pixmap)
    return _CLASS_ICONS[color]


def class_name(name):
    return NAMES.get(name,name)


def pixel_pen(color,width=3):
    pen=QPen(QColor(color),width);pen.setCosmetic(True);pen.setJoinStyle(Qt.RoundJoin)
    return pen


class HaloPolygon(QGraphicsPolygonItem):
    def __init__(self,points,color='#FFCB47',parent=None,closed=True):
        super().__init__(QPolygonF([QPointF(*p) for p in points]),parent)
        self.color=color;self.closed=closed;self._paint_padding=5.0;self.setPen(pixel_pen(color));self.setZValue(5)

    def boundingRect(self):
        margin=getattr(self,'_paint_padding',5.0)
        return self.polygon().boundingRect().adjusted(-margin,-margin,margin,margin)

    def set_view_scale(self,scale):
        # paint() draws up to 7 viewport pixels, independent of scene zoom.
        # The native polygon pen bounds are in scene units and cannot describe that halo.
        margin=5.0/max(abs(scale),.000001)
        if margin==self._paint_padding:return
        self.prepareGeometryChange();self._paint_padding=margin;self.update()

    def paint(self,painter,option,widget=None):
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setPen(pixel_pen('#101820',7 if self.isSelected() else 6));painter.setBrush(Qt.NoBrush)
        draw=painter.drawPolygon if self.closed else painter.drawPolyline
        draw(self.polygon())
        if self.isSelected():
            painter.setPen(pixel_pen('#FFFFFF',6));draw(self.polygon())
        painter.setPen(pixel_pen(self.color,3))
        fill=QColor(self.color);fill.setAlpha(38 if self.isSelected() else 14);painter.setBrush(fill if self.closed else Qt.NoBrush)
        draw(self.polygon())


class ObjectBadge(QGraphicsItem):
    def __init__(self,text,parent):
        super().__init__(parent);self.text=text;self.font=QFont('Microsoft JhengHei',10,QFont.Bold)
        metrics=QFontMetricsF(self.font);self.rect=QRectF(0,-27,min(240,metrics.horizontalAdvance(text)+18),24)
        self.setFlag(QGraphicsItem.ItemIgnoresTransformations);self.setAcceptedMouseButtons(Qt.NoButton);self.setZValue(12)

    def boundingRect(self):return self.rect.adjusted(-1,-1,1,1)

    def paint(self,painter,option,widget=None):
        color='#FFFFFF' if self.parentItem().isSelected() else self.parentItem().color
        painter.setPen(pixel_pen(color,1));painter.setBrush(QColor('#14202D'));painter.drawRoundedRect(self.rect,4,4)
        painter.setFont(self.font);painter.setPen(QColor(color))
        text=QFontMetricsF(self.font).elidedText(self.text,Qt.ElideRight,int(self.rect.width()-12))
        painter.drawText(self.rect.adjusted(7,0,-5,0),Qt.AlignVCenter,text)


class MovablePolygon(HaloPolygon):
    def __init__(self,points,index,canvas,text='',color='#FFCB47'):
        super().__init__(points,color);self.index=index;self.canvas=canvas
        self.setFlags(QGraphicsItem.ItemIsSelectable|QGraphicsItem.ItemIsMovable|QGraphicsItem.ItemSendsGeometryChanges)
        self.badge=ObjectBadge(text,self);self.badge.setPos(min(p[0] for p in points),min(p[1] for p in points))
        self.badge.setVisible(canvas.show_all_labels);self.setToolTip(text)

    def itemChange(self,change,value):
        if change==QGraphicsItem.ItemSelectedHasChanged:
            self.update()
            if hasattr(self,'badge'):self.badge.setVisible(self.isSelected() or self.canvas.show_all_labels);self.badge.update()
        elif change==QGraphicsItem.ItemPositionHasChanged and hasattr(self,'canvas'):
            for handle in self.canvas.handles:
                if handle.index==self.index:
                    point=self.canvas.display[self.index]['points'][handle.vertex]
                    handle.setPos(point[0]+value.x(),point[1]+value.y())
        return super().itemChange(change,value)

    def mousePressEvent(self,event):
        self.canvas.beforeEdit.emit();super().mousePressEvent(event)

    def mouseReleaseEvent(self,event):
        super().mouseReleaseEvent(event)
        if not self.pos().isNull():self.canvas.moved.emit(self.index,self.pos().x(),self.pos().y())


class Vertex(QGraphicsRectItem):
    def __init__(self,canvas,index,vertex,point):
        super().__init__(-9,-9,18,18);self.canvas=canvas;self.index=index;self.vertex=vertex;self.hovered=False
        self.setPos(*point);self.setBrush(QColor('#FFFFFF'));self.setPen(pixel_pen('#1264B6',2));self.setZValue(30)
        self.setFlags(QGraphicsItem.ItemIsMovable|QGraphicsItem.ItemIgnoresTransformations)
        self.setCursor(Qt.OpenHandCursor);self.setAcceptHoverEvents(True)
        self.setToolTip(f'頂點 {vertex+1}：按住左鍵拖曳；點位太密時先放大影像。')

    def paint(self,painter,option,widget=None):
        painter.setRenderHint(QPainter.Antialiasing);painter.setPen(pixel_pen('#1264B6',2))
        painter.setBrush(QColor('#BAE6FD' if self.hovered else '#FFFFFF'));painter.drawRoundedRect(QRectF(-6,-6,12,12),2,2)

    def hoverEnterEvent(self,event):self.hovered=True;self.update();super().hoverEnterEvent(event)

    def hoverLeaveEvent(self,event):self.hovered=False;self.update();super().hoverLeaveEvent(event)

    def mousePressEvent(self,event):
        self.canvas.beforeEdit.emit();super().mousePressEvent(event)

    def mouseReleaseEvent(self,event):
        super().mouseReleaseEvent(event);self.canvas.vertexMoved.emit(self.index,self.vertex,self.pos().x(),self.pos().y())

    def mouseMoveEvent(self,event):
        super().mouseMoveEvent(event);self.canvas.preview_vertex(self.index,self.vertex,self.pos().x(),self.pos().y())
