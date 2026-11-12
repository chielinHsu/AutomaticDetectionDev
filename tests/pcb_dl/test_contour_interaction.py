import copy,os,tempfile,unittest
from unittest.mock import patch
from pathlib import Path
os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
import numpy as np
from PySide6.QtCore import QPoint,QPointF,QSettings,Qt
from PySide6.QtGui import QCloseEvent
from PySide6.QtTest import QSignalSpy,QTest
from PySide6.QtWidgets import QApplication,QGraphicsEllipseItem,QGraphicsItem
from tools.pcb_dl.canvas import Canvas
from tools.pcb_dl.canvas_items import MovablePolygon
from tools.pcb_dl.demo import generate
from tools.pcb_dl.window import Workbench,configure_fonts


class ContourInteractionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app=QApplication.instance() or QApplication([]);configure_fonts(cls.app)

    def canvas(self):
        view=Canvas();view.resize(500,400);view.set_image(np.full((1000,1000,3),(30,60,40),np.uint8))
        view.show();self.app.processEvents();view.set_zoom(100);return view

    def close_view(self,view):view.hide();view.deleteLater();self.app.processEvents()

    def click(self,view,x,y):
        QTest.mouseClick(view.viewport(),Qt.LeftButton,pos=view.mapFromScene(QPointF(x,y)));self.app.processEvents()

    def test_each_polygon_click_has_a_fixed_pixel_anchor_and_backspace_removes_it(self):
        view=self.canvas();view.set_mode('annotation-poly')
        try:
            for count,point in enumerate([(430,430),(550,430),(550,550)],1):
                self.click(view,*point)
                dots=[i for i in view.scene().items() if isinstance(i,QGraphicsEllipseItem)]
                self.assertEqual(len(dots),count,'Each placed vertex must be visibly marked')
                self.assertTrue(all(i.flags() & QGraphicsItem.ItemIgnoresTransformations for i in dots))
            QTest.keyClick(view,Qt.Key_Backspace);self.app.processEvents()
            self.assertEqual(len(view.points),2);self.assertEqual(sum(isinstance(i,QGraphicsEllipseItem) for i in view.scene().items()),2)
            QTest.keyClick(view,Qt.Key_Escape);self.assertEqual(view.points,[])
        finally:self.close_view(view)

    def test_invalid_contour_keeps_all_points_until_corrected(self):
        view=self.canvas();view.set_mode('annotation-poly');drawn=QSignalSpy(view.drawn)
        try:
            for p in [(430,430),(550,550),(430,550),(550,430)]:self.click(view,*p)
            before=copy.deepcopy(view.points);rejected=QSignalSpy(view.drawRejected)
            QTest.keyClick(view,Qt.Key_Return)
            self.assertEqual(drawn.count(),0);self.assertEqual(view.points,before,'Invalid completion must preserve the editable draft')
            self.assertEqual(rejected.count(),1)
        finally:self.close_view(view)

    def test_clicking_start_closes_once_and_returns_to_selection(self):
        view=self.canvas();view.set_mode('annotation-poly');drawn=QSignalSpy(view.drawn)
        try:
            for p in [(430,430),(550,430),(550,550)]:self.click(view,*p)
            self.click(view,430,430)
            self.assertEqual(drawn.count(),1);self.assertEqual(len(drawn.at(0)[0]),3)
            self.assertEqual(view.mode,'select');self.assertEqual(view.points,[])
        finally:self.close_view(view)

    def test_enter_and_keypad_enter_finish_the_contour(self):
        for key in (Qt.Key_Return,Qt.Key_Enter):
            with self.subTest(key=key):
                view=self.canvas();view.set_mode('annotation-poly');drawn=QSignalSpy(view.drawn)
                try:
                    for p in [(430,430),(550,430),(550,550)]:self.click(view,*p)
                    QTest.keyClick(view,key);self.assertEqual(drawn.count(),1);self.assertEqual(view.mode,'select')
                    self.assertIsNone(view.draft);self.assertEqual(view.points,[])
                finally:self.close_view(view)

    def test_selection_moves_frame_and_handles_while_pan_only_moves_view(self):
        view=self.canvas();points=[[430,430],[550,430],[550,550],[430,550]]
        view.render([dict(points=points,shape='polygon',**{'class':'other'})],False);view.choose(0)
        frame=next(i for i in view.scene().items() if isinstance(i,MovablePolygon));moved=QSignalSpy(view.moved)
        try:
            center=view.mapFromScene(QPointF(490,490));bars=(view.horizontalScrollBar().value(),view.verticalScrollBar().value())
            QTest.mousePress(view.viewport(),Qt.LeftButton,pos=center);QTest.mouseMove(view.viewport(),center+QPoint(20,15));self.app.processEvents()
            self.assertAlmostEqual(view.handles[0].scenePos().x(),430+frame.pos().x(),delta=.1)
            QTest.mouseRelease(view.viewport(),Qt.LeftButton,pos=center+QPoint(20,15));self.assertEqual(moved.count(),1)
            self.assertEqual(bars,(view.horizontalScrollBar().value(),view.verticalScrollBar().value()))
            view.set_mode('pan');position=frame.pos();point=view.mapFromScene(frame.mapToScene(QPointF(490,490)))
            QTest.mousePress(view.viewport(),Qt.LeftButton,pos=point);QTest.mouseMove(view.viewport(),point+QPoint(30,20));QTest.mouseRelease(view.viewport(),Qt.LeftButton,pos=point+QPoint(30,20))
            self.assertEqual(frame.pos(),position);self.assertEqual(moved.count(),1)
            self.assertNotEqual(bars,(view.horizontalScrollBar().value(),view.verticalScrollBar().value()))
            view.set_mode('select');bars=(view.horizontalScrollBar().value(),view.verticalScrollBar().value());position=frame.pos()
            QTest.mousePress(view.viewport(),Qt.LeftButton,pos=QPoint(20,20));QTest.mouseMove(view.viewport(),QPoint(40,40));QTest.mouseRelease(view.viewport(),Qt.LeftButton,pos=QPoint(40,40))
            self.assertNotEqual(bars,(view.horizontalScrollBar().value(),view.verticalScrollBar().value()))
            self.assertEqual(frame.pos(),position);self.assertEqual(moved.count(),1)
        finally:self.close_view(view)

    def test_vertex_hit_padding_drags_the_point_instead_of_the_whole_frame(self):
        view=self.canvas();points=[[430,430],[550,430],[550,550],[430,550]]
        view.render([dict(points=points,shape='polygon',**{'class':'other'})],False);view.choose(0)
        vertices=QSignalSpy(view.vertexMoved);bodies=QSignalSpy(view.moved)
        try:
            point=view.mapFromScene(QPointF(430,430))+QPoint(8,0)
            QTest.mousePress(view.viewport(),Qt.LeftButton,pos=point);QTest.mouseMove(view.viewport(),point+QPoint(20,10))
            QTest.mouseRelease(view.viewport(),Qt.LeftButton,pos=point+QPoint(20,10))
            self.assertEqual(vertices.count(),1);self.assertEqual(bodies.count(),0)
            self.assertEqual(vertices.at(0)[0:2],[0,0]);self.assertAlmostEqual(vertices.at(0)[2],450,delta=1)
        finally:self.close_view(view)

    def test_polygon_completion_button_feedback_and_undo(self):
        with tempfile.TemporaryDirectory() as temp:
            w=Workbench(settings=QSettings(str(Path(temp)/'ui.ini'),QSettings.IniFormat));w.open_project_path(generate(Path(temp)/'images'))
            w.show();self.app.processEvents();w.choose_tool('annotation-poly');before=len(w.entry()['annotations'])
            try:
                self.assertTrue(hasattr(w,'finish_contour_button'),'Contour completion needs a visible action')
                self.assertTrue(w.finish_contour_button.isVisible());self.assertFalse(w.finish_contour_button.isEnabled())
                height,width=w.image.shape[:2]
                for x,y in [(width*.2,height*.2),(width*.6,height*.2),(width*.6,height*.6)]:self.click(w.canvas,x,y)
                self.assertTrue(w.finish_contour_button.isEnabled());self.assertIn('3',w.tool_hint.text())
                self.click(w.canvas,width*.2,height*.6)
                w.finish_contour_button.click();self.app.processEvents()
                self.assertEqual(len(w.entry()['annotations']),before+1);self.assertEqual(w.entry()['annotations'][-1]['shape'],'polygon')
                self.assertEqual(w.canvas.mode,'select');self.assertEqual(len(w.canvas.handles),4);self.assertIn('輪廓已完成',w.notice.text())
                w.undo();self.assertEqual(len(w.entry()['annotations']),before)
            finally:self.close_view(w)

    def test_unfinished_draft_is_not_reported_saved_or_silently_discarded(self):
        with tempfile.TemporaryDirectory() as temp:
            w=Workbench(settings=QSettings(str(Path(temp)/'ui.ini'),QSettings.IniFormat));w.open_project_path(generate(Path(temp)/'images'))
            w.show();self.app.processEvents();w.choose_tool('annotation-poly');before=copy.deepcopy(w.project)
            try:
                height,width=w.image.shape[:2];self.click(w.canvas,width*.25,height*.25)
                self.assertIn('草稿',w.save_status.text());self.assertFalse(w.reviewed.isEnabled())
                with self.assertRaisesRegex(ValueError,'繪製尚未完成'):w.save_project()
                event=QCloseEvent()
                with patch('tools.pcb_dl.guidance.QMessageBox.question',side_effect=AssertionError('Keep the draft before the document-close prompt')):w.close_workbench(event)
                self.assertFalse(event.isAccepted());self.assertEqual(len(w.canvas.points),1);self.assertEqual(w.project,before)
                w.cancel_draft_button.click();self.assertEqual(w.canvas.points,[]);self.assertNotIn('草稿',w.save_status.text())
                self.assertEqual(w.project,before);self.assertTrue(w.reviewed.isEnabled())
            finally:self.close_view(w)
