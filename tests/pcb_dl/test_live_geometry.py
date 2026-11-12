"""R09.26: the numeric geometry fields move, resize and rotate the selected rectangle at once."""
import copy
import os
from pathlib import Path
import tempfile
import unittest

os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
from PySide6.QtCore import QSettings
from PySide6.QtWidgets import QApplication
from tools.pcb_dl.canvas_items import MovablePolygon
from tools.pcb_dl.demo import generate
from tools.pcb_dl.inspection import frame_corners,inspection_boxes,preset
from tools.pcb_dl.model import corners,pose_from_polygon
from tools.pcb_dl.window import Workbench,configure_fonts


class LiveGeometryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app=QApplication.instance() or QApplication([]);configure_fonts(cls.app)

    def window(self,temp,inspection=False):
        w=Workbench(settings=QSettings(str(Path(temp)/'ui.ini'),QSettings.IniFormat));w.open_project_path(generate(Path(temp)/'source'))
        if inspection:
            # One lead reaches past the body, so a frame can fit while its dependent frame does not.
            group=preset(28,12);group['boxes'][-1]['pose']['x']+=20;w.project['profiles']['resistor']['inspection']=group;w.refresh()
        w.show();self.app.processEvents();w.table.selectRow(0);w.canvas.choose(0);return w

    def release(self,w):w.hide();w.deleteLater();self.app.processEvents()

    def frame(self,w,index=0):
        return next(i for i in w.canvas.scene().items() if isinstance(i,MovablePolygon) and i.index==index)

    def centre(self,w):
        # Signals blocked: place the fixture frame in the middle without going through the live path.
        fields=w.annotation_fields
        for key,value in dict(x=128,y=128,w=28,h=12,angle=0).items():
            fields[key].blockSignals(True);fields[key].setValue(value);fields[key].blockSignals(False)
        w.apply_annotation_geometry();self.app.processEvents()

    def assert_canvas_shows(self,w,pose,frame):
        """The same frame item is still selected and it, its handles and the document are at `pose`."""
        expected=corners(pose);self.assertIs(self.frame(w),frame,'The scene must not be rebuilt for a live update')
        self.assertTrue(frame.isSelected());self.assertTrue(frame.pos().isNull());self.assertEqual(len(w.canvas.handles),4)
        shown=[frame.mapToScene(p) for p in frame.polygon()]
        for point,want,stored in zip(shown,expected,w.entry()['annotations'][0]['points']):
            self.assertAlmostEqual(point.x(),want[0],delta=.01);self.assertAlmostEqual(point.y(),want[1],delta=.01)
            self.assertAlmostEqual(stored[0],want[0],delta=.01);self.assertAlmostEqual(stored[1],want[1],delta=.01)
        for handle in w.canvas.handles:
            self.assertIs(handle.scene(),w.canvas.scene())
            self.assertAlmostEqual(handle.scenePos().x(),expected[handle.vertex][0],delta=.01)
            self.assertAlmostEqual(handle.scenePos().y(),expected[handle.vertex][1],delta=.01)

    def test_angle_steps_rotate_frame_and_handles_at_once(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.window(temp);self.centre(w);frame=self.frame(w);handles=list(w.canvas.handles);zoom=w.canvas.transform()
            try:
                w.reviewed.setChecked(True)
                for step in range(1,31):
                    w.annotation_fields['angle'].stepBy(1)  # the up arrow, the wheel and Up key all end here
                    self.assert_canvas_shows(w,dict(x=128,y=128,w=28,h=12,angle=step),frame)
                self.assertEqual(w.canvas.handles,handles,'Handles are moved, not recreated');self.assertEqual(w.canvas.transform(),zoom)
                self.assertAlmostEqual(pose_from_polygon(w.entry()['annotations'][0]['points'])['angle'],30,delta=.05)
                self.assertFalse(w.entry()['reviewed']);self.assertFalse(w.reviewed.isChecked())
                self.assertEqual(w.selected_index,0);self.assertNotIn('需要處理',w.notice.text())
            finally:self.release(w)

    def test_centre_and_size_follow_and_dependent_inspection_frames_move_too(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.window(temp,inspection=True);self.centre(w);frame=self.frame(w);fields=w.annotation_fields
            try:
                regions=[c for c in frame.childItems() if hasattr(c,'inspection_pose')];self.assertTrue(regions)
                pose=dict(x=128,y=128,w=28,h=12,angle=0)
                for key,value in [('x',140.5),('y',101.25),('w',40),('h',16),('angle',-20)]:
                    fields[key].setValue(value);pose[key]=value;self.assert_canvas_shows(w,pose,frame)
                    boxes={b['id']:b['pose'] for b in inspection_boxes(pose_from_polygon(corners(pose)),w.project['profiles']['resistor'])}
                    self.assertEqual({r.box_id for r in regions},set(boxes))
                    for region in regions:
                        self.assertIs(region.parentItem(),frame)
                        for point,want in zip(region.polygon(),frame_corners(boxes[region.box_id])):
                            self.assertAlmostEqual(point.x(),want[0],delta=.01);self.assertAlmostEqual(point.y(),want[1],delta=.01)
                # A short side entered as the width stays as typed while adjusting; the frame is what was asked for.
                fields['w'].setValue(10);self.assertAlmostEqual(fields['w'].value(),10);self.assertAlmostEqual(fields['h'].value(),16)
                self.assert_canvas_shows(w,{**pose,'w':10},frame)
            finally:self.release(w)

    def test_invalid_values_keep_the_last_valid_frame_and_say_why(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.window(temp,inspection=True);self.centre(w);frame=self.frame(w);fields=w.annotation_fields
            try:
                before=copy.deepcopy(w.project);shown=list(frame.polygon());history=len(w.history)
                for key,value,message in [('x',-100,'超出影像'),('w',1,'至少需要 2 px'),('w',600,'超出影像')]:
                    with self.subTest(key=key,value=value):
                        old=fields[key].value();fields[key].setValue(value)
                        self.assertEqual(w.project,before);self.assertEqual(list(frame.polygon()),shown);self.assertEqual(len(w.history),history)
                        self.assertIs(self.frame(w),frame);self.assertTrue(frame.isSelected());self.assertEqual(len(w.canvas.handles),4)
                        self.assertIn(message,w.notice.text());self.assertIn('需要處理',w.notice.text());self.assertTrue(w.notice.property('error'))
                        self.assertIn('尚未套用',w.geometry_status.text());self.assertAlmostEqual(fields[key].value(),value)
                        fields[key].blockSignals(True);fields[key].setValue(old);fields[key].blockSignals(False)
                # The body fits but a dependent inspection frame would leave the image.
                box=max(inspection_boxes(dict(x=0,y=0,w=28,h=12,angle=0),w.project['profiles']['resistor']),key=lambda b:b['pose']['x']+b['pose']['w']/2)
                reach=box['pose']['x']+box['pose']['w']/2
                self.assertGreater(reach,15);fields['x'].setValue(256-(14+reach)/2)
                self.assertEqual(w.project,before);self.assertEqual(list(frame.polygon()),shown);self.assertIn('相依檢測框超出影像',w.notice.text())
                fields['x'].blockSignals(True);fields['x'].setValue(128);fields['x'].blockSignals(False)
                # Correcting the value applies it and clears the warning.
                fields['x'].setValue(130);self.assert_canvas_shows(w,dict(x=130,y=128,w=28,h=12,angle=0),frame)
                self.assertNotIn('需要處理',w.notice.text());self.assertFalse(w.notice.property('error'));self.assertIn('幾何有效',w.geometry_status.text())
            finally:self.release(w)

    def test_one_continuous_adjustment_is_one_undo_step(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.window(temp);self.centre(w);fields=w.annotation_fields
            try:
                original=copy.deepcopy(w.entry()['annotations'][0]);history=len(w.history)
                for _ in range(25):fields['angle'].stepBy(1)
                self.assertEqual(len(w.history),history+1,'Holding the arrow is one undo step, not 25')
                w.undo();self.assertEqual(w.entry()['annotations'][0],original);self.assertEqual(len(w.history),history)
                self.assertAlmostEqual(fields['angle'].value(),0,delta=.01);self.assertEqual(w.selected_index,0);self.assertEqual(len(w.canvas.handles),4)
                w.redo_action();self.assertAlmostEqual(pose_from_polygon(w.entry()['annotations'][0]['points'])['angle'],25,delta=.05)
                w.undo();self.assertEqual(w.entry()['annotations'][0],original)
                # After the undo the same field starts a new step; another field is its own step.
                fields['angle'].stepBy(1);fields['angle'].stepBy(1);fields['x'].stepBy(3);fields['x'].stepBy(1)
                self.assertEqual(len(w.history),history+2)
                w.undo();self.assertAlmostEqual(pose_from_polygon(w.entry()['annotations'][0]['points'])['angle'],2,delta=.05)
                self.assertAlmostEqual(pose_from_polygon(w.entry()['annotations'][0]['points'])['x'],128,delta=.01)
                w.undo();self.assertEqual(w.entry()['annotations'][0],original)
                # Enter, Tab or leaving the field (editingFinished) closes the adjustment.
                fields['angle'].stepBy(1);fields['angle'].editingFinished.emit();fields['angle'].stepBy(1)
                self.assertEqual(len(w.history),history+2)
                # The button does not add a step for values that are already applied.
                w.apply_annotation_geometry();self.assertEqual(len(w.history),history+2)
                w.undo();w.undo();self.assertEqual(w.entry()['annotations'][0],original)
            finally:self.release(w)

    def test_fields_written_by_the_program_do_not_edit_the_document(self):
        with tempfile.TemporaryDirectory() as temp:
            w=self.window(temp);fields=w.annotation_fields
            try:
                before=copy.deepcopy(w.project);history=len(w.history)
                for row in (1,2,0):
                    w.table.selectRow(row);w.canvas.choose(row);self.app.processEvents()
                    pose=pose_from_polygon(w.entry()['annotations'][row]['points'])
                    self.assertAlmostEqual(fields['x'].value(),pose['x'],delta=.01);self.assertAlmostEqual(fields['angle'].value(),pose['angle'],delta=.01)
                    self.assertEqual(w.project,before);self.assertEqual(len(w.history),history)
                # Dragging the frame and undoing write the fields back without a second edit.
                w.remember();w.moved(0,3.0,2.0);moved=copy.deepcopy(w.entry()['annotations'][0])
                self.assertAlmostEqual(fields['x'].value(),pose_from_polygon(moved['points'])['x'],delta=.01)
                self.assertEqual(w.entry()['annotations'][0],moved);self.assertEqual(len(w.history),history+1)
                w.undo();self.assertEqual(w.entry()['annotations'],before['images'][0]['annotations']);self.assertEqual(len(w.history),history);self.assertEqual(w.redo.__len__(),1)
                # A polygon has no width/height/angle fields: values there change nothing.
                w.table.selectRow(0);w.canvas.choose(0);w.convert_annotation_shape('polygon');self.app.processEvents()
                polygon=copy.deepcopy(w.entry()['annotations'][0]);steps=len(w.history)
                self.assertFalse(fields['angle'].isEnabled());fields['angle'].setValue(fields['angle'].value()+10)
                self.assertEqual(w.entry()['annotations'][0],polygon);self.assertEqual(len(w.history),steps)
            finally:self.release(w)


if __name__=='__main__':unittest.main()
