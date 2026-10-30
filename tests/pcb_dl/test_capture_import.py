import json
from pathlib import Path
import tempfile
import unittest

import cv2
import numpy as np

from tools.pcb_dl.capture_import import CHANNEL_FILES,import_capture
from tools.pcb_dl.dataset import export_dataset


class CaptureImportTests(unittest.TestCase):
    def create_capture(self,root):
        for view,offset in [('z0',0),('z+1',10),('z-1',20)]:
            folder=root/view;folder.mkdir(parents=True)
            image=np.full((40,60,3),(30+offset,70,120),np.uint8)
            cv2.imencode('.png',image)[1].tofile(folder/(view+'.png'))
            for channel,file in zip((2,1,0),CHANNEL_FILES):
                cv2.imencode('.bmp',image[:,:,channel])[1].tofile(folder/file)

    def test_combined_channels_and_safe_grouping(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'captures';self.create_capture(root)
            file,report=import_capture(root,Path(temp)/'import','board-001')
            project=json.loads(file.read_text());self.assertEqual(len(project['images']),3)
            self.assertEqual({e['group'] for e in project['images']},{'board-001'})
            self.assertTrue(all(not e['reviewed'] and not e['annotations'] for e in project['images']))
            self.assertTrue(all(c['exact_match'] for view in report['views'] for c in view['channels'].values()))
            for entry in project['images']:entry['reviewed']=True
            with self.assertRaisesRegex(ValueError,'獨立 group'):export_dataset(project,Path(temp)/'dataset')
            with self.assertRaisesRegex(ValueError,'必須為空'):import_capture(root,file.parent,'board-001')
            with self.assertRaisesRegex(ValueError,'之外'):import_capture(root,root/'output','board-001')

    def test_wrong_merge_or_missing_channel_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'captures';self.create_capture(root)
            target=root/'z0'/CHANNEL_FILES[0]
            cv2.imencode('.bmp',np.zeros((40,60),np.uint8))[1].tofile(target)
            with self.assertRaisesRegex(ValueError,'不一致'):import_capture(root,Path(temp)/'wrong','board-001')
            target.unlink()
            with self.assertRaisesRegex(ValueError,'缺少通道'):import_capture(root,Path(temp)/'missing','board-001')
