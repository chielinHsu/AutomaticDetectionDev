import copy
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
import numpy as np
from PySide6.QtCore import QPointF,Qt
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication
from tools.pcb_dl.annotation_geometry import annotation_shape,is_rectangle,resized_vertex
from tools.pcb_dl.demo import generate
from tools.pcb_dl.model import corners
from tools.pcb_dl.window import Workbench,configure_fonts

RECT=[[20,20],[60,20],[60,40],[20,40]]


class GeometryTests(unittest.TestCase):
    def test_all_corners_and_rotated_rectangles_keep_fixed_opposite_and_right_angles(self):
        rotated=corners({'x':100,'y':100,'w':60,'h':30,'angle':27})
        for points in [RECT,rotated]:
            for i in range(4):
                label={'points':points,'shape':'rectangle'}
                result=resized_vertex(label,i,points[i][0]+7,points[i][1]-5)
                self.assertTrue(is_rectangle(result));np.testing.assert_allclose(result[(i+2)%4],points[(i+2)%4])
                self.assertEqual(label['points'],points)

    def test_crossing_corner_and_minimum_size_do_not_create_degenerate_polygon(self):
        for target in [(80,60),(60,40),(60.1,40.1)]:
            result=resized_vertex({'points':RECT,'shape':'rectangle'},0,*target)
            self.assertTrue(is_rectangle(result));self.assertGreaterEqual(min(np.linalg.norm(np.roll(result,-1,axis=0)-result,axis=1)),1.999)

    def test_legacy_inference_and_explicit_polygon_intent(self):
        self.assertEqual(annotation_shape({'points':RECT}),'rectangle')
        skew=[[20,20],[60,23],[55,40],[20,40]]
        self.assertEqual(annotation_shape({'points':skew}),'polygon')
        poly={'points':RECT,'shape':'polygon'};changed=resized_vertex(poly,0,12,15)
        self.assertFalse(is_rectangle(changed));self.assertEqual(changed[1:],RECT[1:])
        with self.assertRaises(ValueError):annotation_shape({'points':skew,'shape':'rectangle'})


class ShapeUiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app=QApplication.instance() or QApplication([]);configure_fonts(cls.app)

    def make_window(self,temp):
        w=Workbench();w.open_project_path(generate(Path(temp)/'source'));w.show();self.app.processEvents()
        w.choose_tool('annotation-rect');w.drawn(copy.deepcopy(RECT),False);return w

    def release(self,w):w.hide();w.deleteLater();self.app.processEvents()

    def test_rectangle_draw_and_vertex_edit_keep_rectangle_and_undo(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.make_window(temp);index=len(w.entry()['annotations'])-1;label=w.entry()['annotations'][index]
            self.assertEqual(label.get('shape'),'rectangle')
            w.remember();w.vertex_moved(index,0,12,15);self.assertTrue(is_rectangle(label['points']))
            self.assertEqual(label['points'][0],[12,15]);self.assertEqual(label['points'][2],RECT[2])
            w.undo();self.assertEqual(w.entry()['annotations'][index]['points'],RECT)
            file=Path(temp)/'rect.json'
            with patch('tools.pcb_dl.workflow.QFileDialog.getSaveFileName',return_value=(str(file),'')):w.save_project(True)
            w.open_project_path(file);self.assertEqual(w.entry()['annotations'][index]['shape'],'rectangle');self.release(w)

    def test_explicit_shape_conversion_free_edit_roundtrip_and_boundary_rejection(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.make_window(temp);index=w.selected_index
            w.convert_annotation_shape('polygon');w.undo();self.assertEqual(w.entry()['annotations'][index]['shape'],'rectangle')
            w.convert_annotation_shape('polygon');w.remember();w.vertex_moved(index,0,12,15)
            label=w.entry()['annotations'][index];self.assertFalse(is_rectangle(label['points']))
            self.assertEqual(label['points'][1:],RECT[1:]);self.assertEqual(label['shape'],'polygon')
            file=Path(temp)/'saved.json'
            with patch('tools.pcb_dl.workflow.QFileDialog.getSaveFileName',return_value=(str(file),'')):w.save_project(True)
            w.open_project_path(file);self.assertEqual(w.entry()['annotations'][index]['shape'],'polygon')
            w.select(index);w.canvas.choose(index);w.convert_annotation_shape('rectangle')
            label=w.entry()['annotations'][index];self.assertTrue(is_rectangle(label['points']))
            original=copy.deepcopy(label);w.remember()
            with self.assertRaises(ValueError):w.vertex_moved(index,0,-100,-100)
            self.assertEqual(label,original);self.release(w)

    def test_mouse_drag_previews_and_commits_rectangle(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.make_window(temp);index=w.selected_index
            a=w.canvas.mapFromScene(QPointF(*RECT[0]));b=w.canvas.mapFromScene(QPointF(12,15))
            QTest.mousePress(w.canvas.viewport(),Qt.LeftButton,pos=a);QTest.mouseMove(w.canvas.viewport(),b)
            self.assertEqual(w.canvas.mode,'select')
            self.app.processEvents()
            item=next(i for i in w.canvas.scene().items() if getattr(i,'index',None)==index and hasattr(i,'polygon'))
            preview=[[p.x(),p.y()] for p in item.polygon()]
            self.assertTrue(is_rectangle(preview));self.assertLess(preview[0][0],RECT[0][0])
            QTest.mouseRelease(w.canvas.viewport(),Qt.LeftButton,pos=b)
            self.assertTrue(is_rectangle(w.entry()['annotations'][index]['points']));self.release(w)
