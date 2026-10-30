"""Decode thumbnails off the UI thread; queued results belong to a live window."""
from PySide6.QtCore import QObject,QRunnable,QSize,QThreadPool,Signal
from PySide6.QtGui import QImage,QImageReader

_POOL=None


class ThumbnailSignals(QObject):
    ready=Signal(str,QImage)


class ThumbnailJob(QRunnable):
    def __init__(self,path,cancelled):
        super().__init__();self.path=path;self.cancelled=cancelled;self.signals=ThumbnailSignals()

    def run(self):
        image=QImage()
        if not self.cancelled.is_set():
            reader=QImageReader(self.path);reader.setScaledSize(QSize(72,50));image=reader.read()
        self.signals.ready.emit(self.path,image)


def request_thumbnail(window,path):
    global _POOL
    if _POOL is None:_POOL=QThreadPool();_POOL.setMaxThreadCount(2)
    job=ThumbnailJob(path,window._thumbnail_cancel);window._thumbnail_jobs[path]=job
    job.signals.ready.connect(window.thumbnail_ready);_POOL.start(job)
