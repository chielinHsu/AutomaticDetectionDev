import copy
import os
from pathlib import Path
import tempfile
import unittest

os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
from PySide6.QtCore import QPointF,Qt
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication
from tools.pcb_dl.demo import generate
from tools.pcb_dl.model import pose_from_polygon
from tools.pcb_dl.window import Workbench,configure_fonts


class DeleteKeyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app=QApplication.instance() or QApplication([]);configure_fonts(cls.app)

    def make_window(self,temp):
        w=Workbench();w.open_project_path(generate(Path(temp)/'source'));w.show();self.app.processEvents();return w

    def cleanup_window(self,w):
        w.hide();w.deleteLater();self.app.processEvents()

    def test_delete_selected_annotation_in_load_step_and_undo(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.make_window(temp);w.go_step(0);original=copy.deepcopy(w.entry()['annotations'])
            pose=pose_from_polygon(original[0]['points']);point=w.canvas.mapFromScene(QPointF(pose['x'],pose['y']))
            QTest.mouseClick(w.canvas.viewport(),Qt.LeftButton,pos=point)
            self.assertEqual(w.selected_index,0)
            QTest.keyClick(w.canvas.viewport(),Qt.Key_Delete)
            self.assertEqual(len(w.entry()['annotations']),len(original)-1)
            self.assertFalse(w.entry()['reviewed']);w.undo();self.assertEqual(w.entry()['annotations'],original)
            self.cleanup_window(w)

    def test_delete_from_object_table_and_no_selection_is_safe(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.make_window(temp);original=copy.deepcopy(w.entry()['annotations'])
            w.table.selectRow(1);w.table.setFocus();QTest.keyClick(w.table,Qt.Key_Delete)
            self.assertEqual(w.entry()['annotations'],[original[0],original[2]])
            QTest.keyClick(w.table,Qt.Key_Delete);self.assertEqual(len(w.entry()['annotations']),2)
            self.cleanup_window(w)

    def test_delete_in_text_field_edits_text_not_selected_frame(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.make_window(temp);original=copy.deepcopy(w.entry()['annotations']);w.select(0);w.canvas.choose(0);w.go_step(3)
            w.group.setText('ABC');w.group.setCursorPosition(0);w.group.setFocus();QTest.keyClick(w.group,Qt.Key_Delete)
            self.assertEqual(w.group.text(),'BC');self.assertEqual(w.entry()['annotations'],original)
            self.cleanup_window(w)

    def test_delete_selected_cad_is_undoable(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.make_window(temp);w.objects=[{'class':'resistor','pose':{'x':60,'y':60,'w':28,'h':12,'angle':0},'manual':True,'confidence':0}]
            w.go_step(4);w.select(0);w.canvas.choose(0);w.canvas.setFocus();QTest.keyClick(w.canvas,Qt.Key_Delete)
            self.assertEqual(w.objects,[]);w.undo();self.assertEqual(len(w.objects),1)
            self.cleanup_window(w)

    def test_delete_does_not_remove_existing_frame_during_polygon_draft(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.make_window(temp);original=copy.deepcopy(w.entry()['annotations']);w.select(0);w.canvas.choose(0)
            w.choose_tool('annotation-poly');w.canvas.points=[[10,10],[20,20]]
            QTest.keyClick(w.canvas,Qt.Key_Delete);self.assertEqual(w.entry()['annotations'],original)
            self.assertIn('完成輪廓',w.notice.text())
            QTest.keyClick(w.canvas,Qt.Key_Backspace);self.assertEqual(w.canvas.points,[[10,10]])
            self.cleanup_window(w)
