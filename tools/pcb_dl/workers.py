import threading
from PySide6.QtCore import QThread,Signal


class InferenceWorker(QThread):
    completed=Signal(list,dict)
    failed=Signal(str)
    progress=Signal(int,int)

    def __init__(self,manifest,image,roi,confidence,tile,parent=None,profiles=None):
        super().__init__(parent);self.args=(manifest,image,roi,confidence,tile);self.profiles=profiles;self.cancelled=threading.Event()

    def run(self):
        try:
            from .inference import LearningEngine
            manifest,image,roi,confidence,tile=self.args
            engine=LearningEngine(manifest,self.profiles)
            result=engine.predict(image,roi,confidence,tile,cancel=self.cancelled.is_set,progress=self.progress.emit)
            if not self.cancelled.is_set():self.completed.emit(result,engine.spec)
        except Exception as exc:self.failed.emit(str(exc))
