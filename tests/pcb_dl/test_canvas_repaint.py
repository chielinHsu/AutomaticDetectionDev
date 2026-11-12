"""Read the backing bitmap directly; QWidget.grab() would hide stale-pixel bugs."""
import os
from pathlib import Path
import unittest
os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
import numpy as np
from PySide6.QtCore import QPoint,QPointF,Qt
from PySide6.QtGui import QColor,QImage
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication,QGraphicsView
from tools.pcb_dl.canvas import Canvas


class CanvasRepaintTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.app=QApplication.instance() or QApplication([])

    def make_canvas(self,zoom):
        view=Canvas();view.resize(460,380)
        self.assertNotEqual(view.viewportUpdateMode(),QGraphicsView.FullViewportUpdate)
        view.set_image(np.full((1000,1000,3),(32,64,40),np.uint8));view.show();QTest.qWait(15);view.set_zoom(zoom);QTest.qWait(10)
        return view

    def point(self,view,x,y):
        rect=view.mapFromScene(view.sceneRect()).boundingRect().intersected(view.viewport().rect()).adjusted(10,10,-10,-10)
        return QPoint(round(rect.left()+rect.width()*x),round(rect.top()+rect.height()*y))

    def capture(self,view):
        device=view.backingStore().paintDevice()
        self.assertIsInstance(device,QImage,'Run the pixel regression on the offscreen raster platform')
        image=device.copy().convertToFormat(QImage.Format_RGBA8888)
        self.assertEqual(image.width(),int(view.width()*view.devicePixelRatioF()+.5))
        data=np.frombuffer(image.constBits(),np.uint8).reshape(image.height(),image.bytesPerLine())[:,:image.width()*4].copy()
        return image,data

    def assert_clean(self,view,label):
        QTest.qWait(8);partial,a=self.capture(view)
        view.viewport().repaint();QTest.qWait(3);full,b=self.capture(view)
        changed=int(np.any(a.reshape(-1,4)!=b.reshape(-1,4),axis=1).sum())
        if changed:
            folder=Path(__file__).resolve().parents[2]/'build/r09-repaint-failures';folder.mkdir(parents=True,exist_ok=True)
            partial.save(str(folder/(label+'-partial.png')));full.save(str(folder/(label+'-full.png')))
        self.assertEqual(changed,0,f'{label}: {changed} stale pixels removed only by a full repaint')

    def release(self,view):
        QTest.mouseRelease(view.viewport(),Qt.LeftButton,pos=self.point(view,.5,.5))
        view.hide();view.deleteLater();self.app.processEvents()

    def test_rectangle_draft_expansion_contraction_and_escape(self):
        for zoom in (9,15,24,100,200):
            with self.subTest(zoom=zoom):
                view=self.make_canvas(zoom)
                try:
                    view.set_mode('annotation-rect');QTest.mousePress(view.viewport(),Qt.LeftButton,pos=self.point(view,.2,.2))
                    for step,(x,y) in enumerate([(.8,.8),(.5,.6),(.7,.4),(.4,.7)]):
                        QTest.mouseMove(view.viewport(),self.point(view,x,y));self.assert_clean(view,f'draft-{zoom}-{step}')
                    QTest.keyClick(view,Qt.Key_Escape);self.assert_clean(view,f'cancel-{zoom}')
                finally:self.release(view)

    def test_vertex_resize_has_no_trails_during_motion(self):
        for zoom in (9,15,24,100,200):
            with self.subTest(zoom=zoom):
                view=self.make_canvas(zoom)
                try:
                    a=view.mapToScene(self.point(view,.2,.2));b=view.mapToScene(self.point(view,.8,.8))
                    points=[[a.x(),a.y()],[b.x(),a.y()],[b.x(),b.y()],[a.x(),b.y()]]
                    view.render([dict(points=points,shape='rectangle',**{'class':'resistor'})],False);view.choose(0);QTest.qWait(10)
                    QTest.mousePress(view.viewport(),Qt.LeftButton,pos=view.mapFromScene(QPointF(*points[0])))
                    for step,(x,y) in enumerate([(.35,.3),(.45,.4),(.15,.25),(.3,.2)]):
                        QTest.mouseMove(view.viewport(),self.point(view,x,y));self.assert_clean(view,f'vertex-{zoom}-{step}')
                finally:self.release(view)

    def test_open_contour_anchors_rubber_band_backspace_and_escape(self):
        for zoom in (9,24,100):
            with self.subTest(zoom=zoom):
                view=self.make_canvas(zoom)
                try:
                    view.set_mode('annotation-poly')
                    for step,(x,y) in enumerate([(.2,.2),(.8,.2),(.8,.8)]):
                        QTest.mouseClick(view.viewport(),Qt.LeftButton,pos=self.point(view,x,y))
                        QTest.mouseMove(view.viewport(),self.point(view,.4,.5));self.assert_clean(view,f'contour-{zoom}-{step}')
                    QTest.keyClick(view,Qt.Key_Backspace);self.assert_clean(view,f'contour-backspace-{zoom}')
                    QTest.keyClick(view,Qt.Key_Escape);self.assert_clean(view,f'contour-cancel-{zoom}')
                finally:self.release(view)

    def test_textured_background_does_not_shift_sampling_during_draft_changes(self):
        view=Canvas();view.resize(883,624)
        view.set_image(np.random.default_rng(7).integers(0,256,(1024,1280,3),dtype=np.uint8));view.show();QTest.qWait(20);view.fit_image()
        def point(x,y):return view.mapFromScene(QPointF(x*1280,y*1024))
        try:
            view.set_mode('annotation-rect');QTest.mousePress(view.viewport(),Qt.LeftButton,pos=point(.2,.2))
            for step,(x,y) in enumerate([(.8,.8),(.5,.55),(.7,.65),(.4,.7)]):
                QTest.mouseMove(view.viewport(),point(x,y));self.assert_clean(view,f'texture-{step}')
        finally:self.release(view)

    def test_cached_image_and_theme_background_are_replaced_without_old_pixels(self):
        view=self.make_canvas(15)
        try:
            view.set_image(np.full((1000,1000,3),(80,140,200),np.uint8));QTest.qWait(10)
            image,_=self.capture(view);point=view.viewport().mapTo(view,view.mapFromScene(QPointF(500,500)))
            ratio=view.devicePixelRatioF();color=image.pixelColor(round(point.x()*ratio),round(point.y()*ratio))
            self.assertEqual((color.red(),color.green(),color.blue()),(200,140,80))
            view.set_zoom(15);view.setBackgroundBrush(QColor('#122F45'));QTest.qWait(10)
            image,_=self.capture(view);point=view.viewport().mapTo(view,QPoint(10,10))
            color=image.pixelColor(round(point.x()*ratio),round(point.y()*ratio))
            self.assertEqual((color.red(),color.green(),color.blue()),(18,47,69))
        finally:self.release(view)
