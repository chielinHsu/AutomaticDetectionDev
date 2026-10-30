import copy
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
from PySide6.QtCore import Qt
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication
from tools.pcb_dl.canvas_items import MovablePolygon
from tools.pcb_dl.demo import generate
from tools.pcb_dl.window import Workbench,configure_fonts


class ObjectBrowserTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app=QApplication.instance() or QApplication([]);configure_fonts(cls.app)

    def window(self,temp):
        w=Workbench();w.open_project_path(generate(Path(temp)/'source'));w.show();self.app.processEvents();return w

    def release(self,w):w.hide();w.deleteLater();self.app.processEvents()

    def test_groups_start_collapsed_and_restore_after_refresh_and_query(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.window(temp);original=copy.deepcopy(w.project)
            self.assertEqual(len(w.table.groups),3);self.assertTrue(all(not g.isExpanded() for g in w.table.groups.values()))
            w.table.groups['resistor'].setExpanded(True);w.refresh();self.assertTrue(w.table.groups['resistor'].isExpanded())
            w.object_search.setText('IC');self.assertTrue(w.table.groups['ic'].isExpanded());self.assertTrue(w.table.groups['resistor'].isHidden())
            w.object_search.clear();self.assertTrue(w.table.groups['resistor'].isExpanded());self.assertFalse(w.table.groups['ic'].isExpanded())
            w.table.collapseAll();w.refresh();self.assertTrue(all(not g.isExpanded() for g in w.table.groups.values()))
            self.assertEqual(w.project,original);self.release(w)

    def test_search_class_name_and_exact_number_filters_canvas_without_data_change(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.window(temp);labels=w.entry()['annotations'];labels[0]['name']='R22'
            labels.extend(copy.deepcopy(labels[0]) for _ in range(9));w.refresh();original=copy.deepcopy(w.project)
            for query,expected in [('電容',{1}),('R22',{0,*range(3,12)}),('#1',{0}),('#12',{11}),('#999',set())]:
                w.object_search.setText(query)
                visible={item.index for item in w.canvas.scene().items() if isinstance(item,MovablePolygon) and item.isVisible()}
                self.assertEqual(visible,expected,query)
            self.assertIn('沒有符合項目',w.search_status.text());w.object_search.clear()
            self.assertEqual(sum(not item.isHidden() for item in w.table.rows.values()),12)
            self.assertEqual(w.project,original);self.release(w)

    def test_select_and_double_click_locate_and_only_selected_label_is_visible(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.window(temp);original=copy.deepcopy(w.project);before=w.canvas.transform().m11()
            w.table.selectRow(2);self.app.processEvents();self.assertEqual(w.selected_index,2)
            items=[i for i in w.canvas.scene().items() if isinstance(i,MovablePolygon)]
            self.assertEqual({i.index for i in items if i.badge.isVisible()},{2})
            point=w.table.visualItemRect(w.table.rows[2]).center();QTest.mouseClick(w.table.viewport(),Qt.LeftButton,pos=point);QTest.mouseDClick(w.table.viewport(),Qt.LeftButton,pos=point)
            self.assertGreater(w.canvas.transform().m11(),before);self.assertIn('放大定位',w.notice.text())
            w.all_labels.setChecked(True);self.assertTrue(all(i.badge.isVisible() for i in items))
            self.assertEqual(w.project,original);self.release(w)

    def test_class_header_is_not_a_delete_target_and_search_delete_only_edits_text(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.window(temp);original=copy.deepcopy(w.entry()['annotations'])
            w.table.selectRow(0);w.table.setCurrentItem(w.table.groups['resistor']);self.assertEqual(w.selected_index,-1)
            w.table.setFocus();QTest.keyClick(w.table,Qt.Key_Delete);self.assertEqual(w.entry()['annotations'],original)
            w.table.selectRow(0);w.object_search.setText('resistor');w.object_search.setFocus();w.object_search.setCursorPosition(0)
            QTest.keyClick(w.object_search,Qt.Key_Delete);self.assertEqual(w.object_search.text(),'esistor')
            self.assertEqual(w.entry()['annotations'],original);self.release(w)

    def test_name_save_undo_and_whole_list_toggle(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.window(temp);w.table.selectRow(0);w.object_name.setText('R22');w.rename_selected()
            self.assertEqual(w.entry()['annotations'][0]['name'],'R22');w.undo()
            self.assertNotIn('name',w.entry()['annotations'][0]);w.table.selectRow(0);w.object_name.setText('R22');w.rename_selected()
            file=Path(temp)/'named.json'
            with patch('tools.pcb_dl.workflow.QFileDialog.getSaveFileName',return_value=(str(file),'')):w.save_project(True)
            w.open_project_path(file);w.object_search.setText('R22');self.assertFalse(w.table.rows[0].isHidden())
            self.assertEqual(w.entry()['annotations'][0]['name'],'R22');w.toggle_object_list();self.assertFalse(w.object_panel.isVisible())
            w.toggle_object_list();self.assertTrue(w.object_panel.isVisible());self.release(w)
