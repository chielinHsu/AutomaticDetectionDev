import copy
import json
import math
import os
from pathlib import Path
import tempfile
import unittest

os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
os.environ.setdefault('YOLO_CONFIG_DIR',str(Path('build/pcb-dl-settings').resolve()))
os.environ.setdefault('YOLO_AUTOINSTALL','False')

import cv2
import numpy as np

from tools.pcb_dl.demo import generate
from tools.pcb_dl.dataset import export_dataset,verify_dataset
from tools.pcb_dl.inference import LearningEngine,suppress,tile_origins
from tools.pcb_dl.model import atomic_json,corners,make_recipe,pose_from_polygon,validate_project


def fixture(root,task):
    """Constant ONNX backend fixture, explicitly NOT a trained PCB model."""
    import onnx
    from onnx import TensorProto,helper,numpy_helper
    from tools.pcb_dl.model import CLASSES
    data=np.zeros((1,8 if task=='obb' else 9,10),dtype=np.float32)
    data[0,:4,0]=[32,32,24,12];data[0,4,0]=.95
    data[0,7,0]=.3 if task=='obb' else 1
    nodes=[helper.make_node('Constant',[],['output0'],value=numpy_helper.from_array(data))]
    outputs=[helper.make_tensor_value_info('output0',TensorProto.FLOAT,list(data.shape))]
    if task=='segment':
        proto=np.ones((1,2,16,16),dtype=np.float32)
        nodes.append(helper.make_node('Constant',[],['output1'],value=numpy_helper.from_array(proto)))
        outputs.append(helper.make_tensor_value_info('output1',TensorProto.FLOAT,list(proto.shape)))
    graph=helper.make_graph(nodes,'fixture',[helper.make_tensor_value_info('images',TensorProto.FLOAT,[1,3,64,64])],outputs)
    model=helper.make_model(graph,opset_imports=[helper.make_opsetid('',17)]);model.ir_version=10
    helper.set_model_props(model,{'task':task,'names':str(dict(enumerate(CLASSES))),'stride':'32','imgsz':'[64,64]','batch':'1'})
    path=Path(root)/(task+'.onnx');onnx.save(model,path)
    import hashlib
    spec={'format':'pcb-dl-model','version':1,'task':task,'model':path.name,'imgsz':64,
          'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'classes':CLASSES,
          'profiles':{n:{'w':20,'h':10,'window_ratio':.6} for n in CLASSES},'demo_only':True}
    manifest=path.with_suffix('.json');atomic_json(manifest,spec);return manifest


class DataTests(unittest.TestCase):
    def test_rotation_and_fixed_cad_corners(self):
        for angle in [-80,-30,0,25,75]:
            p={'x':50,'y':60,'w':30,'h':10,'angle':angle};out=pose_from_polygon(corners(p))
            self.assertAlmostEqual(out['x'],50,places=4);self.assertAlmostEqual(out['w'],30,places=4)
            self.assertAlmostEqual(out['angle'],angle,places=4)

    def test_reviewed_export_both_tasks_and_group_leak_rejection(self):
        with tempfile.TemporaryDirectory() as temp:
            project=json.loads(generate(Path(temp)/'source').read_text())
            self.assertEqual(validate_project(project,True),{'train':8,'val':2,'test':2})
            verify_dataset(Path(temp)/'source/dataset')
            for task,columns in [('obb',9),('segment',9)]:
                files=list((Path(temp)/'source/dataset'/task/'labels/train').glob('*.txt'))
                self.assertEqual(len(files),8);self.assertEqual(len(files[0].read_text().splitlines()[0].split()),columns)
            wrong=copy.deepcopy(project);wrong['images'][-1]['group']=wrong['images'][0]['group']
            with self.assertRaisesRegex(ValueError,'不可跨'):validate_project(wrong,True)
            wrong=copy.deepcopy(project);wrong['images'][0]['reviewed']=False
            with self.assertRaisesRegex(ValueError,'審核'):export_dataset(wrong,Path(temp)/'bad')
            wrong=copy.deepcopy(project);wrong['images'][0]['annotations'][0]['points'][0]=[-1,3]
            with self.assertRaises(ValueError):validate_project(wrong)
            wrong=copy.deepcopy(project);wrong['images'][-1]['path']=wrong['images'][0]['path']
            with self.assertRaisesRegex(ValueError,'重複影像'):validate_project(wrong)
            label=next((Path(temp)/'source/dataset/obb/labels/train').glob('*.txt'))
            label.write_text('0 0 0 1 0 1 1 0 1\n')
            with self.assertRaisesRegex(ValueError,'已變更'):verify_dataset(Path(temp)/'source/dataset')

    def test_tiles_and_rotated_nms(self):
        self.assertEqual(tile_origins(2300,1024,.25),[0,768,1276])
        pose={'x':50,'y':50,'w':30,'h':10,'angle':25}
        base={'class':'ic','observed':pose,'confidence':.9}
        other={**base,'confidence':.7}
        self.assertEqual(len(suppress([other,base,{**base,'class':'resistor'}])),2)

    def test_recipe_roundtrip_and_outside_rejection(self):
        profiles={'ic':{'w':40,'h':20,'window_ratio':.6}}
        obj={'class':'ic','pose':{'x':50,'y':50,'w':40,'h':20,'angle':30},'manual':True}
        recipe=make_recipe('image.png',100,100,profiles,[obj],{'task':'obb'})
        with tempfile.TemporaryDirectory() as temp:
            file=Path(temp)/'配方.json';atomic_json(file,recipe);self.assertEqual(json.loads(file.read_text()),recipe)
        obj['pose']['x']=0
        with self.assertRaisesRegex(ValueError,'超出'):make_recipe('x',100,100,profiles,[obj],{})

    def test_actual_onnx_both_tasks_and_cad_spec(self):
        with tempfile.TemporaryDirectory() as temp:
            for task in ['obb','segment']:
                manifest=fixture(temp,task);profiles=json.loads(manifest.read_text())['profiles'];profiles['resistor']['w']=33
                engine=LearningEngine(manifest,profiles);image=np.zeros((128,128,3),np.uint8)
                result=engine.predict(image,[32,32,64,64],tile=64)
                self.assertEqual(len(result),1,task)
                self.assertAlmostEqual(result[0]['pose']['x'],64,delta=2)
                self.assertEqual(result[0]['pose']['w'],33)
                self.assertEqual(result[0]['pose']['h'],10)
                self.assertGreaterEqual(len(result[0]['outline']),4)
                if task=='obb':self.assertAlmostEqual(result[0]['pose']['angle'],-math.degrees(.3),places=3)
                with self.assertRaises(InterruptedError):engine.predict(image,[0,0,64,64],tile=64,cancel=lambda:True)

    def test_model_hash_and_missing_profile_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            manifest=fixture(temp,'obb');spec=json.loads(manifest.read_text());spec['profiles'].pop('ic');atomic_json(manifest,spec)
            with self.assertRaisesRegex(ValueError,'CAD'):LearningEngine(manifest)
            spec['profiles']['ic']={'w':20,'h':10,'window_ratio':.6};spec['sha256']='bad';atomic_json(manifest,spec)
            with self.assertRaisesRegex(ValueError,'SHA'):LearningEngine(manifest)
            spec=json.loads(fixture(temp,'obb').read_text());spec['task']='segment';atomic_json(manifest,spec)
            with self.assertRaisesRegex(ValueError,'task'):LearningEngine(manifest)


class UiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from PySide6.QtWidgets import QApplication
        cls.app=QApplication.instance() or QApplication([])
        from tools.pcb_dl.window import configure_fonts
        configure_fonts(cls.app)

    def test_annotation_drag_cad_edit_undo_and_recipe(self):
        from tools.pcb_dl.window import Workbench
        with tempfile.TemporaryDirectory() as temp:
            project=json.loads(generate(Path(temp)/'source').read_text())
            window=Workbench();window.project=project;window.profile_table();window.reload_images()
            before=len(window.entry()['annotations']);window.drawn([[10,10],[30,10],[30,20],[10,20]],False)
            self.assertEqual(len(window.entry()['annotations']),before+1);self.assertFalse(window.entry()['reviewed'])
            window.undo();self.assertEqual(len(window.entry()['annotations']),before)
            window.redo_action();self.assertEqual(len(window.entry()['annotations']),before+1)
            spec=json.loads(fixture(temp,'obb').read_text())
            obj={'class':'resistor','pose':{'x':64,'y':64,'w':20,'h':10,'angle':20},'observed':{'x':64,'y':64,'w':24,'h':12,'angle':20},'confidence':.9,'outline':[],'manual':False}
            window.apply_inference([obj],spec,[0,0,256,256]);window.select(0)
            window.fields['w'].setValue(25);window.fields['angle'].setValue(35);window.apply_geometry()
            self.assertEqual(window.objects[0]['pose']['w'],25);self.assertTrue(window.objects[0]['manual'])
            window.apply_inference([copy.deepcopy(obj)],spec,[0,0,256,256]);self.assertEqual(len(window.objects),1)
            self.assertEqual(window.objects[0]['pose']['angle'],35)
            from unittest.mock import patch
            recipe=Path(temp)/'recipe.json'
            with patch('tools.pcb_dl.workflow.QFileDialog.getSaveFileName',return_value=(str(recipe),'')):
                window.save_recipe()
            with patch('tools.pcb_dl.workflow.QFileDialog.getOpenFileName',return_value=(str(recipe),'')):
                window.open_recipe()
            self.assertEqual(window.objects[0]['pose']['w'],25)
            self.assertEqual(window.objects[0]['pose']['angle'],35)
            self.assertTrue(window.objects[0]['manual'])
            window.grab().save(str(Path('build/r09-dl-ui.png').resolve()))
            window.deleteLater();self.app.processEvents()

    def test_mouse_annotation_vertex_and_background_onnx(self):
        from PySide6.QtCore import QPointF,Qt
        from PySide6.QtTest import QTest
        from tools.pcb_dl.window import Workbench
        with tempfile.TemporaryDirectory() as temp:
            project=json.loads(generate(Path(temp)/'source').read_text())
            window=Workbench();window.project=project;window.profile_table();window.reload_images();window.show();self.app.processEvents()
            before=len(window.entry()['annotations']);window.canvas.set_mode('annotation-rect')
            a=window.canvas.mapFromScene(QPointF(15,15));b=window.canvas.mapFromScene(QPointF(45,35))
            QTest.mousePress(window.canvas.viewport(),Qt.LeftButton,pos=a)
            QTest.mouseMove(window.canvas.viewport(),b)
            QTest.mouseRelease(window.canvas.viewport(),Qt.LeftButton,pos=b)
            self.assertEqual(len(window.entry()['annotations']),before+1)
            window.canvas.set_mode('select');window.canvas.choose(before)
            self.assertEqual(len(window.canvas.handles),4)
            window.remember();window.vertex_moved(before,0,12,12)
            self.assertEqual(window.entry()['annotations'][before]['points'][0],[12,12])
            window.manifest=str(fixture(temp,'segment'));window.task.setCurrentText('segment');window.tile.setValue(64)
            window.infer([32,32,64,64])
            with self.assertRaisesRegex(ValueError,'推論'):window.switch_image(1)
            for _ in range(400):
                QTest.qWait(50)
                if not window.worker.isRunning():break
            self.assertFalse(window.worker.isRunning());self.app.processEvents()
            self.assertEqual(len(window.objects),1)
            self.assertEqual(window.provenance['task'],'segment')
            window.grab().save(str(Path('build/r09-dl-inference-ui.png').resolve()))
            window.hide();window.deleteLater();self.app.processEvents()


if __name__=='__main__':unittest.main()
