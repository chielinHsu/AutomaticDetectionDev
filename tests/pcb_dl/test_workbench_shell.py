import copy
import json
import os
from pathlib import Path
import tempfile
import unittest

os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
from PySide6.QtCore import QSettings,Qt
from PySide6.QtGui import QPalette
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication
from tools.pcb_dl.canvas_items import MovablePolygon,class_color
from tools.pcb_dl.demo import generate
from tools.pcb_dl.window import Workbench,configure_fonts


class WorkbenchShellTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app=QApplication.instance() or QApplication([]);configure_fonts(cls.app)

    def window(self,temp,load=True):
        settings=QSettings(str(Path(temp)/'appearance.ini'),QSettings.IniFormat)
        w=Workbench(settings=settings)
        if load:w.open_project_path(generate(Path(temp)/'source'))
        w.show();self.app.processEvents();return w

    def release(self,w):w.hide();w.deleteLater();self.app.processEvents()

    def test_default_dark_persisted_light_and_local_palette(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.window(temp,False);self.assertEqual(w.theme_mode,'dark')
            w.theme_choice.setCurrentIndex(1);self.assertEqual(w.theme_mode,'light')
            self.assertGreater(w.palette().color(QPalette.Window).lightness(),200)
            other=Workbench(settings=QSettings(str(Path(temp)/'appearance.ini'),QSettings.IniFormat))
            self.assertEqual(other.theme_mode,'light');self.release(other);self.release(w)

    def test_theme_switch_preserves_draft_selection_pixels_and_view(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.window(temp);w.table.selectRow(0);w.canvas.zoom_by(1.7);w.choose_tool('annotation-poly')
            w.canvas.points=[[15,15],[40,15]];w.canvas.draw_draft(w.canvas.points)
            before=copy.deepcopy(w.project);pixels=w.image.copy();transform=w.canvas.transform();draft=w.canvas.draft;handles=list(w.canvas.handles)
            for mode in ('light','dark'):
                w.change_theme(mode);self.app.processEvents()
                self.assertEqual(w.project,before);self.assertTrue((w.image==pixels).all());self.assertEqual(w.selected_index,0)
                self.assertEqual(w.canvas.transform(),transform);self.assertIs(w.canvas.draft,draft)
                self.assertEqual(w.canvas.points,[[15,15],[40,15]]);self.assertEqual(w.canvas.handles,handles)
            w.step_buttons[2].click();self.assertEqual(w.step,1);self.assertIn('尚未完成',w.notice.text())
            self.assertEqual(sum(b.isChecked() for b in w.step_buttons),1);self.release(w)

    def test_rectangle_property_transaction_review_crop_and_boundary(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.window(temp);w.table.selectRow(0);w.canvas.choose(0);original=copy.deepcopy(w.entry()['annotations'][0])
            self.assertFalse(w.crop_preview.pixmap().isNull());w.reviewed.setChecked(True)
            fields=w.annotation_fields;fields['w'].setValue(20);fields['h'].setValue(12);fields['x'].setValue(40);fields['y'].setValue(40)
            w.apply_annotation_geometry();self.assertFalse(w.entry()['reviewed']);self.assertIn('幾何有效',w.geometry_status.text())
            self.assertAlmostEqual(w.annotation_fields['w'].value(),20,places=1);w.undo();self.assertEqual(w.entry()['annotations'][0],original)
            w.table.selectRow(0);before=copy.deepcopy(w.project);fields['x'].setValue(-100)
            with self.assertRaisesRegex(ValueError,'超出影像'):w.apply_annotation_geometry()
            self.assertEqual(w.project,before);w.clear_selection();self.assertFalse(w.annotation_details.isVisible());self.assertFalse(w.delete_button.isEnabled())
            w.table.selectRow(0);self.assertFalse(w.crop_preview.pixmap().isNull());self.release(w)

    def test_thumbnail_navigation_and_class_color_identity(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.window(temp);self.assertEqual(w.image_list.count(),len(w.project['images']))
            self.assertIn('3 標註',w.image_list.item(0).text());w.image_list.setCurrentRow(1)
            self.assertEqual(w.current,1);self.assertIn(Path(w.entry()['path']).name,w.image_title.text())
            w.image_previous.click();self.assertEqual(w.current,0)
            frames=[i for i in w.canvas.scene().items() if isinstance(i,MovablePolygon)]
            for frame in frames:self.assertEqual(frame.color,class_color(w.entry()['annotations'][frame.index]['class']))
            w.table.selectRow(0);w.choose_tool('pan');self.assertFalse(w.canvas.isInteractive());self.assertEqual(w.canvas.mode,'pan')
            before=copy.deepcopy(w.project);point=w.canvas.mapFromScene(w.canvas.handles[0].pos())
            QTest.mouseClick(w.canvas.viewport(),Qt.LeftButton,pos=point);self.assertEqual(w.canvas.mode,'pan');self.assertEqual(w.project,before)
            w.choose_tool('select');self.assertTrue(w.canvas.isInteractive());self.release(w)

    def test_custom_components_are_not_restricted_to_pcb(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.window(temp);w.project['classes'].append('screw');w.profile_table();w.refresh();w.table.selectRow(0)
            w.selected_class.setCurrentIndex(w.selected_class.findData('screw'));w.reclassify_selected()
            self.assertEqual(w.entry()['annotations'][0]['class'],'screw');self.assertNotIn('PCB',w.windowTitle())
            self.assertIn('screw',w.table.groups);self.release(w)
