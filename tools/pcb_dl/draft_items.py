"""Open contour preview with screen-sized anchors and a separate tentative segment."""
from PySide6.QtCore import QLineF,QPointF,Qt
from PySide6.QtGui import QColor,QPolygonF
from PySide6.QtWidgets import QGraphicsEllipseItem,QGraphicsItem,QGraphicsLineItem
from .canvas_items import HaloPolygon,pixel_pen


class ContourDraft(HaloPolygon):
    def __init__(self,points):
        super().__init__(points,'#FFCB47',closed=False);self.setZValue(15);self.setAcceptedMouseButtons(Qt.NoButton)
        self.markers=[];self.anchor_points=()
        self.preview=QGraphicsLineItem(self);self.preview.setAcceptedMouseButtons(Qt.NoButton);self.preview.setZValue(1)
        self.set_points(points)

    def set_points(self,points):
        values=tuple(map(tuple,points))
        if values==self.anchor_points:return
        self.anchor_points=values;self.setPolygon(QPolygonF([QPointF(*p) for p in points]))
        while len(self.markers)>len(points):
            marker=self.markers.pop()
            if marker.scene() is not None:marker.scene().removeItem(marker)
            marker.setParentItem(None)
        for index,point in enumerate(points):
            if index==len(self.markers):
                marker=QGraphicsEllipseItem(-5,-5,10,10,self);marker.setFlag(QGraphicsItem.ItemIgnoresTransformations)
                marker.setAcceptedMouseButtons(Qt.NoButton);marker.setPen(pixel_pen('#14202D',2));marker.setZValue(2);self.markers.append(marker)
            marker=self.markers[index];marker.setPos(*point);marker.setBrush(QColor('#22C55E' if index==0 else '#FFFFFF'))
            marker.setToolTip('起點：至少三點後按此閉合輪廓' if index==0 else f'草稿頂點 {index+1}')

    def set_cursor(self,point,closing=False):
        self.preview.setVisible(point is not None and bool(self.anchor_points))
        if point is None or not self.anchor_points:return
        self.preview.setLine(QLineF(QPointF(*self.anchor_points[-1]),QPointF(*point)))
        pen=pixel_pen('#22C55E' if closing else '#FFCB47',2);pen.setStyle(Qt.DashLine);self.preview.setPen(pen)
