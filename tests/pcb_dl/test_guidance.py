import copy
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
import numpy as np
from PySide6.QtCore import QPoint,QPointF,Qt
from PySide6.QtGui import QCloseEvent
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication,QMessageBox
from tools.pcb_dl.canvas import Canvas
from tools.pcb_dl.canvas_items import MovablePolygon
from tools.pcb_dl.demo import generate
from tools.pcb_dl.window import Workbench,configure_fonts


class GuidanceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app=QApplication.instance() or QApplication([]);configure_fonts(cls.app)

    def loaded(self,temp):
        path=generate(Path(temp)/'source');w=Workbench();w.open_project_path(path);w.show();self.app.processEvents();return w

    def release(self,w):
        w.hide();w.deleteLater();self.app.processEvents()

    def test_empty_state_and_missing_model_explain_next_action(self):
        w=Workbench();w.show();self.app.processEvents()
        self.assertEqual(w.pages.count(),5);self.assertEqual(w.image_stack.currentIndex(),1)
        self.assertFalse(w.start_annotation.isEnabled());w.go_step(4)
        self.assertFalse(w.generate_button.isEnabled());self.assertIn('載入元件',w.notice.text())
        self.assertFalse(w.train_advanced.isChecked());self.assertFalse(w.infer_advanced.isChecked());self.release(w)

    def test_mouse_annotation_feedback_class_correction_and_profile_measurement(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.loaded(temp);before=len(w.entry()['annotations'])
            QTest.mouseClick(w.draw_button,Qt.LeftButton);self.assertEqual(w.canvas.mode,'annotation-rect')
            self.assertTrue(w.tool_buttons['annotation-rect'].isChecked())
            a=w.canvas.mapFromScene(QPointF(15,15));b=w.canvas.mapFromScene(QPointF(45,35))
            QTest.mousePress(w.canvas.viewport(),Qt.LeftButton,pos=a);QTest.mouseMove(w.canvas.viewport(),b)
            QTest.mouseRelease(w.canvas.viewport(),Qt.LeftButton,pos=b)
            self.assertEqual(len(w.entry()['annotations']),before+1);self.assertIn('已新增',w.notice.text())
            self.assertEqual(w.selected_index,before);self.assertIn('電阻',w.notice.text())
            w.canvas.scene().clearSelection();self.assertEqual(w.selected_index,-1)
            w.select(before);w.canvas.choose(before)
            w.kind.setCurrentIndex(w.kind.findData('capacitor'));w.reclassify()
            self.assertEqual(w.entry()['annotations'][-1]['class'],'capacitor');self.assertFalse(w.entry()['reviewed'])
            w.go_step(2);w.measure_profile();row=w.project['classes'].index('capacitor')
            self.assertAlmostEqual(float(w.profiles.item(row,1).text()),30,delta=1)
            w.undo();self.assertEqual(w.entry()['annotations'][-1]['class'],'resistor');self.release(w)

    def test_one_board_cannot_be_exported_as_independent_splits(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.loaded(temp)
            for entry in w.project['images']:entry.update(group='one-board',split='train',reviewed=True)
            w.group.setText('one-board');w.reviewed.setChecked(True);w.go_step(3)
            self.assertIn('1 個獨立來源',w.data_status.text());self.assertFalse(w.export_button.isEnabled())
            self.assertFalse(w.train_button.isEnabled());self.release(w)

    def test_accidental_tiny_drag_explains_how_to_zoom(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.loaded(temp);count=len(w.entry()['annotations']);w.choose_tool('annotation-rect')
            a=w.canvas.mapFromScene(QPointF(15,15));b=a+QPoint(2,2)
            QTest.mousePress(w.canvas.viewport(),Qt.LeftButton,pos=a);QTest.mouseMove(w.canvas.viewport(),b)
            QTest.mouseRelease(w.canvas.viewport(),Qt.LeftButton,pos=b)
            self.assertEqual(len(w.entry()['annotations']),count);self.assertIn('先放大',w.notice.text());self.release(w)

    def test_saved_project_and_recipe_cancel_do_not_discard_work(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.loaded(temp);before=copy.deepcopy(w.project)
            self.assertEqual(w.signature(),w.saved_project)
            w.drawn([[10,10],[30,10],[30,20],[10,20]],False)
            w.project_file=None
            event=QCloseEvent()
            with patch('tools.pcb_dl.guidance.QMessageBox.question',return_value=QMessageBox.Save),patch('tools.pcb_dl.workflow.QFileDialog.getSaveFileName',return_value=('','')):
                w.close_workbench(event)
            self.assertFalse(event.isAccepted());self.assertEqual(len(w.entry()['annotations']),len(before['images'][0]['annotations'])+1)
            path=Path(temp)/'saved.json'
            with patch('tools.pcb_dl.workflow.QFileDialog.getSaveFileName',return_value=(str(path),'')):w.save_project(True)
            self.assertIn('已儲存',w.notice.text());self.assertEqual(w.signature(),w.saved_project);self.release(w)

    def test_frame_width_stays_visible_at_full_board_and_half_zoom(self):
        view=Canvas();view.resize(1000,750);view.set_image(np.full((4096,5120,3),(25,65,30),np.uint8))
        points=[[800,700],[4000,700],[4000,1100],[800,1100]]
        view.render([{'class':'resistor','points':points}],False);view.show();self.app.processEvents();view.fitInView(view.sceneRect(),Qt.KeepAspectRatio)
        items=[item for item in view.scene().items() if isinstance(item,MovablePolygon)]
        self.assertTrue(items[0].pen().isCosmetic());self.assertEqual(items[0].pen().widthF(),3)
        widths=[]
        for factor in [1,.5]:
            if factor!=1:view.scale(factor,factor)
            self.app.processEvents();point=view.mapFromScene(QPointF(3000,700));shot=view.viewport().grab().toImage()
            colors=[shot.pixelColor(point.x(),point.y()+dy) for dy in range(-7,8)]
            width=sum(c.blue()>180 and 70<c.green()<180 and c.red()<130 for c in colors);widths.append(width)
            self.assertGreaterEqual(width,2)
        self.assertLessEqual(abs(widths[0]-widths[1]),1)
        view.choose(0);self.assertTrue(items[0].isSelected());self.assertEqual(len(view.handles),4)
        self.assertEqual(view.handles[0].rect().width(),10);view.hide();view.deleteLater();self.app.processEvents()
