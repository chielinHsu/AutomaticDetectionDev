import copy
import json
import math
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
os.environ.setdefault('YOLO_CONFIG_DIR',str(Path('build/pcb-dl-settings').resolve()))
os.environ.setdefault('YOLO_AUTOINSTALL','False')

import cv2
import numpy as np
from PySide6.QtCore import QPointF,QSettings,Qt
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication
from tools.pcb_dl.inspection import (compose,relative,frame_corners,inspection_boxes,preset,validate_inspection)
from tools.pcb_dl.inference import LearningEngine
from tools.pcb_dl.model import atomic_json,corners,make_recipe,new_project,pose_from_polygon,validate_project
from tools.pcb_dl.window import Workbench,configure_fonts
from tools.pcb_dl.canvas_items import HaloPolygon,MovablePolygon
from test_workbench import fixture


class InspectionGeometryTests(unittest.TestCase):
    def test_dataset_retains_template_but_exports_only_component_learning_labels(self):
        from tools.pcb_dl.demo import generate
        from tools.pcb_dl.dataset import export_dataset,verify_dataset
        with tempfile.TemporaryDirectory() as temp:
            project=json.loads(generate(Path(temp)/'source').read_text())
            project['profiles']['ic']['inspection']=preset(20,10)
            destination=export_dataset(project,Path(temp)/'with-inspection')
            stored=json.loads((destination/'source-project.json').read_text())
            self.assertEqual(stored['profiles'],project['profiles']);verify_dataset(destination)
            # Inspection subregions are rules, not extra class-labelled training objects.
            for task in ('obb','segment'):
                old=sum(len(p.read_text().splitlines()) for p in (Path(temp)/'source/dataset'/task/'labels').rglob('*.txt'))
                new=sum(len(p.read_text().splitlines()) for p in (destination/task/'labels').rglob('*.txt'))
                self.assertEqual(old,new)

    def test_nested_frames_and_body_motion_use_rigid_local_coordinates(self):
        component=dict(x=300,y=240,w=140,h=100,angle=90)
        profile=dict(w=140,h=100,window_ratio=.6,inspection=preset(140,100))
        boxes=profile['inspection']['boxes'];boxes[0]['pose'].update(x=7,y=-3,angle=25)
        boxes.append(dict(id='solder',parent='pin-1',kind='window',pose=dict(x=2,y=-4,w=6,h=8,angle=12)))
        result={b['id']:b for b in inspection_boxes(component,profile)}
        body=result['body']['pose'];self.assertAlmostEqual(body['x'],297);self.assertAlmostEqual(body['y'],233)
        self.assertAlmostEqual(body['angle'],115)
        for box in boxes[1:]:
            parent=result[box['parent']]['pose'];local=relative(parent,result[box['id']]['pose'])
            for key,value in box['pose'].items():self.assertAlmostEqual(local[key],value)
        before=copy.deepcopy(result);boxes[0]['pose']['x']+=11;boxes[0]['pose']['angle']+=30
        after={b['id']:b for b in inspection_boxes(component,profile)}
        self.assertAlmostEqual(after['body']['pose']['y'],before['body']['pose']['y']-11)
        self.assertAlmostEqual(after['pin-1']['pose']['angle'],before['pin-1']['pose']['angle']+30)
        # Component envelope size does not scale the authored geometry.
        other=inspection_boxes({**component,'w':500,'h':400},profile)
        self.assertEqual(other,list(after.values()))

    def test_invalid_parent_cycle_duplicate_nonfinite_and_capacity_rejected(self):
        base=preset(140,100)
        for mutate in [lambda g:g['boxes'][1].update(parent='missing'),
                       lambda g:g['boxes'][1].update(id='body'),
                       lambda g:g['boxes'][1]['pose'].update(w=0),
                       lambda g:g['boxes'][0]['pose'].update(x=float('nan')),
                       lambda g:g.update(version=99),
                       lambda g:g.update(boxes=g['boxes']*30)]:
            wrong=copy.deepcopy(base);mutate(wrong)
            with self.assertRaises(ValueError):validate_inspection(wrong)
        cycle=copy.deepcopy(base)
        cycle['boxes']+= [dict(id='a',kind='window',parent='b',pose=copy.deepcopy(base['boxes'][0]['pose'])),
                          dict(id='b',kind='window',parent='a',pose=copy.deepcopy(base['boxes'][0]['pose']))]
        with self.assertRaisesRegex(ValueError,'循環'):validate_inspection(cycle)

    def test_recipe_legacy_roundtrip_regenerates_frames_and_checks_child_boundary(self):
        pose=dict(x=100,y=100,w=120,h=80,angle=-30)
        profile=dict(w=120,h=80,window_ratio=.6,inspection=preset(120,80))
        obj=dict(class_='ic',pose=pose,manual=True);obj['class']=obj.pop('class_')
        recipe=make_recipe('board.png',240,240,{'ic':profile},[obj],{})
        self.assertEqual(len(recipe['objects'][0]['inspection_boxes']),9)
        self.assertEqual(recipe['objects'][0]['inspection_boxes'][1]['parent'],'body')
        recipe['objects'][0]['inspection_boxes'][0]['pose']['x']=-999
        saved=make_recipe('board.png',240,240,{'ic':profile},recipe['objects'],{})
        self.assertEqual(saved['objects'][0]['inspection_boxes'][0]['pose']['x'],100)
        legacy=copy.deepcopy(profile);legacy.pop('inspection')
        self.assertNotIn('inspection_boxes',make_recipe('x',240,240,{'ic':legacy},[obj],{})['objects'][0])
        wrong=copy.deepcopy(profile);wrong['inspection']['boxes'][1]['pose']['x']=400
        with self.assertRaisesRegex(ValueError,'相依檢測框超出'):make_recipe('x',240,240,{'ic':wrong},[obj],{})
        self.assertEqual(json.loads(json.dumps(saved)),saved)

    def test_real_onnx_obb_and_segment_return_nine_related_regions(self):
        with tempfile.TemporaryDirectory() as temp:
            for task in ('obb','segment'):
                manifest=fixture(temp,task);profiles=json.loads(manifest.read_text())['profiles']
                profiles['resistor']['inspection']=preset(20,10)
                engine=LearningEngine(manifest,profiles)
                result=engine.predict(np.zeros((128,128,3),np.uint8),[32,32,64,64],tile=64)
                self.assertEqual(len(result),1)
                boxes=result[0]['inspection_boxes'];self.assertEqual(len(boxes),9)
                self.assertEqual(sum(b['kind']=='pin' and b['parent']=='body' for b in boxes),8)
                self.assertAlmostEqual(boxes[0]['pose']['angle'],result[0]['pose']['angle'])
                profiles['resistor']['inspection']['boxes'][1]['pose']['x']=200
                outside=LearningEngine(manifest,profiles)
                self.assertEqual(outside.predict(np.zeros((128,128,3),np.uint8),[32,32,64,64],tile=64),[])


class InspectionEditorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app=QApplication.instance() or QApplication([]);configure_fonts(cls.app)

    def window(self,temp):
        path=Path(temp)/'board.png';cv2.imencode('.png',np.full((600,800,3),35,np.uint8))[1].tofile(path)
        project=new_project();project['classes']=['ic'];project['profiles']={'ic':dict(w=160,h=120,window_ratio=.6)}
        project['images']=[dict(path=str(path),width=800,height=600,group='one',split='train',reviewed=False,
            annotations=[dict(class_='ic',points=corners(dict(x=x,y=280,w=160,h=120,angle=0)),shape='rectangle') for x in (240,550)])]
        for label in project['images'][0]['annotations']:label['class']=label.pop('class_')
        file=Path(temp)/'project.json';atomic_json(file,project)
        w=Workbench(settings=QSettings(str(Path(temp)/'theme.ini'),QSettings.IniFormat));w.open_project_path(file)
        w.go_step(2);w.table.selectRow(0);w.canvas.choose(0);w.show();self.app.processEvents();return w,file

    def release(self,w):w.hide();w.deleteLater();self.app.processEvents()

    def test_preset_shared_preview_body_edit_undo_redo_and_persistence(self):
        with tempfile.TemporaryDirectory() as temp:
            w,file=self.window(temp);w.inspection_preset(8)
            self.assertEqual(len(w.canvas.display[0]['inspection_boxes']),9)
            self.assertEqual(len(w.canvas.display[1]['inspection_boxes']),9)
            before=copy.deepcopy(w.project);w.inspection_fields['x'].setValue(12);w.inspection_fields['angle'].setValue(20)
            w.apply_inspection_geometry();after=copy.deepcopy(w.project)
            poses=w.canvas.display[0]['inspection_boxes'];self.assertAlmostEqual(poses[0]['pose']['x'],252)
            self.assertAlmostEqual(poses[1]['pose']['angle'],20)
            w.undo();self.assertEqual(w.project,before);w.redo_action();self.assertEqual(w.project,after)
            w.sync_profiles();w.profile_values();self.assertIn('inspection',w.project['profiles']['ic'])
            w.save_project();stored=json.loads(file.read_text());self.assertEqual(stored,w.project);validate_project(stored)
            w.open_project_path(file);w.go_step(2);w.table.selectRow(0)
            self.assertEqual(w.project,after);self.assertEqual(len(w.canvas.display[1]['inspection_boxes']),9)
            self.release(w)

    def test_mouse_teaches_body_eight_pins_and_attached_window_without_new_annotations(self):
        with tempfile.TemporaryDirectory() as temp:
            w,_=self.window(temp);canvas=w.canvas
            def draw(kind,x,y,width,height):
                w.begin_inspection(kind)
                a=canvas.mapFromScene(QPointF(x,y));b=canvas.mapFromScene(QPointF(x+width,y+height))
                QTest.mousePress(canvas.viewport(),Qt.LeftButton,pos=a);QTest.mouseMove(canvas.viewport(),b)
                QTest.mouseRelease(canvas.viewport(),Qt.LeftButton,pos=b);self.app.processEvents()
                self.assertEqual(canvas.mode,'select',w.last_notice)
            draw('body',180,260,120,40)
            for y in (230,310):
                for x in (180,210,240,270):draw('pin',x,y,15,15)
            self.assertEqual(len(w.entry()['annotations']),2)
            boxes=w.project['profiles']['ic']['inspection']['boxes'];self.assertEqual(len(boxes),9)
            pin=w.inspection_tree.topLevelItem(0).child(0);w.inspection_tree.setCurrentItem(pin)
            draw('window',180,215,12,12)
            self.assertEqual(w.project['profiles']['ic']['inspection']['boxes'][-1]['parent'],'pin-1')
            w.inspection_tree.setCurrentItem(w.inspection_tree.topLevelItem(0).child(0));w.delete_inspection()
            ids={b['id'] for b in w.project['profiles']['ic']['inspection']['boxes']}
            self.assertNotIn('pin-1',ids);self.assertNotIn('window-1',ids)
            w.undo();self.assertEqual(len(w.project['profiles']['ic']['inspection']['boxes']),10)
            self.release(w)

    def test_invalid_edit_atomic_cancel_and_model_load_preserve_authored_group(self):
        with tempfile.TemporaryDirectory() as temp:
            w,_=self.window(temp);w.inspection_preset(8);before=copy.deepcopy(w.project);history=len(w.history)
            w.inspection_fields['x'].setValue(900)
            with self.assertRaisesRegex(ValueError,'超出'):w.apply_inspection_geometry()
            self.assertEqual(w.project,before);self.assertEqual(len(w.history),history)
            w.begin_inspection('pin');QTest.keyClick(w.canvas,Qt.Key_Escape)
            self.assertEqual(w.project,before)
            # Loading a trained model must not discard the independently taught package.
            manifest=fixture(temp,'obb');w.load_model(str(manifest))
            self.assertEqual(w.project['profiles']['ic'],before['profiles']['ic'])
            self.release(w)

    def test_manual_cad_move_rotation_recipe_and_old_expansion_are_consistent(self):
        with tempfile.TemporaryDirectory() as temp:
            w,_=self.window(temp);w.inspection_preset(8);w.go_step(4);w.kind.setCurrentIndex(w.kind.findData('ic'))
            w.canvas.set_mode('cad-rect');w.drawn([[200,200],[280,200],[280,260],[200,260]],False)
            w.select(0);w.canvas.choose(0);self.assertEqual(len(w.canvas.display[0]['inspection_boxes']),9)
            w.fields['x'].setValue(350);w.fields['angle'].setValue(60);w.apply_geometry()
            boxes=w.canvas.display[0]['inspection_boxes'];self.assertAlmostEqual(boxes[0]['pose']['angle'],60)
            file=Path(temp)/'recipe.json'
            with patch('tools.pcb_dl.workflow.QFileDialog.getSaveFileName',return_value=(str(file),'')):w.save_recipe()
            saved=json.loads(file.read_text());self.assertEqual(len(saved['objects'][0]['inspection_boxes']),9)
            with patch('tools.pcb_dl.workflow.QFileDialog.getOpenFileName',return_value=(str(file),'')):w.open_recipe()
            self.assertEqual(w.canvas.display[0]['inspection_boxes'],saved['objects'][0]['inspection_boxes'])
            w.remember();w.moved(0,20,-10)
            self.assertAlmostEqual(w.canvas.display[0]['inspection_boxes'][0]['pose']['x'],370)
            self.assertAlmostEqual(w.canvas.display[0]['inspection_boxes'][0]['pose']['y'],220)
            w.undo();self.assertAlmostEqual(w.objects[0]['pose']['x'],350)
            self.release(w)


if __name__=='__main__':unittest.main()
