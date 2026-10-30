import copy
import sys
import threading
from pathlib import Path

from PySide6.QtCore import QProcess,Qt
from PySide6.QtGui import QAction,QFont,QFontDatabase,QKeySequence,QShortcut
from PySide6.QtWidgets import (QApplication,QCheckBox,QComboBox,QDockWidget,QDoubleSpinBox,QFormLayout,
    QHBoxLayout,QLabel,QLineEdit,QMainWindow,QMessageBox,QPlainTextEdit,QPushButton,QSpinBox,QTableWidget,
    QTableWidgetItem,QVBoxLayout,QWidget,QScrollArea,QHeaderView)

from .canvas import Canvas
from .model import new_project
from .workflow import Workflow
from .guidance import Guidance
from .guided_ui import build_ui
from .workbench_theme import apply_theme,theme_settings
from .workbench_presenter import WorkbenchPresentation
from .canvas_items import class_name
from .object_browser import ObjectBrowserActions


class Workbench(WorkbenchPresentation,ObjectBrowserActions,Guidance,Workflow,QMainWindow):
    def __init__(self,settings=None):
        super().__init__();self.settings=settings if settings is not None else theme_settings();self.setWindowTitle('Vision CAD Studio — 元件深度學習 CAD');self.resize(1440,940);self.setMinimumSize(1060,700)
        self.project=new_project();self.current=-1;self.image=None;self.objects=[];self.provenance={}
        self.manifest='';self.worker=None;self.process=None;self.history=[];self.redo=[];self.selected_index=-1
        self.setup_guidance();self.canvas=Canvas()
        self._thumbnail_cancel=threading.Event();self._thumbnail_jobs={}
        self.destroyed.connect(lambda *_args,event=self._thumbnail_cancel:event.set())
        build_ui(self);apply_theme(self)
        self.canvas.drawn.connect(self.safe(self.drawn));self.canvas.moved.connect(self.safe(self.moved))
        self.canvas.beforeEdit.connect(self.remember);self.canvas.selected.connect(self.select)
        self.canvas.vertexMoved.connect(self.safe(self.vertex_moved));self.canvas.modeChanged.connect(self.tool_changed)
        self.canvas.selectionCleared.connect(self.clear_selection)
        self.canvas.drawRejected.connect(self.show_notice)
        self.profiles.itemChanged.connect(self.update_guidance);self.kind.currentIndexChanged.connect(self.update_guidance)
        self.task.currentIndexChanged.connect(self.update_guidance)
        self.delete_shortcuts=[]
        for target in (self.canvas,self.table):
            shortcut=QShortcut(QKeySequence('Delete'),target);shortcut.setContext(Qt.WidgetWithChildrenShortcut)
            shortcut.activated.connect(self.safe(self.delete));self.delete_shortcuts.append(shortcut)
        self.canvas.zoomChanged.connect(self.update_canvas_status)
        for title,callback,shortcut in [('開啟標註',self.open_project,'Ctrl+O'),('儲存進度',self.save_current,'Ctrl+S'),
            ('另存標註',lambda:self.save_project(True),'Ctrl+Shift+S'),
            ('復原',self.undo,'Ctrl+Z'),('重做',self.redo_action,'Ctrl+Shift+Z')]:
            action=QAction(title,self);action.triggered.connect(self.safe(callback));action.setShortcut(shortcut);self.addAction(action)
            control=QPushButton(title);control.clicked.connect(action.trigger);self.global_actions.addWidget(control)
        self.profile_table();self._ready=True;self.go_step(0)

    def button(self,layout,text,callback):
        button=QPushButton(text);button.clicked.connect(self.safe(callback));layout.addWidget(button)

    def safe(self,callback):
        def wrapped(*args):
            try:
                value=callback(*args) if callback in (self.drawn,self.moved,self.vertex_moved,self.switch_image) else callback()
                self.update_guidance();return value
            except Exception as exc:
                self.log.appendPlainText(str(exc));self.show_notice(str(exc),True);self.update_guidance()
        return wrapped

    def profile_table(self):
        self.profiles.blockSignals(True)
        self.profiles.setRowCount(len(self.project['classes']))
        for row,name in enumerate(self.project['classes']):
            p=self.project['profiles'].get(name,{})
            for col,value in enumerate([name,p.get('w',''),p.get('h',''),p.get('window_ratio',.6)]):
                item=QTableWidgetItem(class_name(name) if col==0 else str(value))
                if col==0:item.setFlags(item.flags() & ~Qt.ItemIsEditable)
                self.profiles.setItem(row,col,item)
        self.profiles.blockSignals(False)
        previous=self.kind.currentData();self.kind.clear()
        for name in self.project['classes']:self.kind.addItem(class_name(name)+' ('+name+')',name)
        index=self.kind.findData(previous)
        if index>=0:self.kind.setCurrentIndex(index)
        self.update_guidance()

    def refresh(self,*_):
        if not self._ready:return
        cad=self.view.currentIndex()==1;objects=self.objects if cad else self.entry().get('annotations',[])
        self.table.populate(objects,self.project['classes'],cad,self.selected_index)
        display=copy.deepcopy(objects)
        if cad:
            for obj in display:obj['window_ratio']=self.project['profiles'].get(obj['class'],{}).get('window_ratio',.6)
        selected=self.selected_index
        self.canvas.render(display,cad)
        if 0<=selected<len(objects):self.canvas.choose(selected)
        else:self.selected_index=-1
        self.statusBar().showMessage(f'{len(objects)} 個'+(' CAD' if cad else '人工標註'))
        self.update_guidance()

    def select(self,index):
        self.table.ensure_filter(index)
        self.selected_index=index;self.table.blockSignals(True);self.table.selectRow(index);self.table.blockSignals(False)
        self.update_guidance()
        if self.view.currentIndex()==1 and 0<=index<len(self.objects):
            for key,spin in self.fields.items():spin.setValue(self.objects[index]['pose'][key])

    def table_selection(self):
        index=self.table.currentRow()
        if index>=0:self.select(index);self.canvas.choose(index)
        else:self.selected_index=-1;self.canvas.choose(-1);self.update_guidance()

    def closeEvent(self,event):
        self.close_workbench(event)
        if event.isAccepted():self._thumbnail_cancel.set()


def configure_fonts(app):
    font=Path('C:/Windows/Fonts/msjh.ttc')
    if font.exists():QFontDatabase.addApplicationFont(str(font))
    app.setFont(QFont('Microsoft JhengHei',10))


def run(task='obb',project=None):
    app=QApplication.instance() or QApplication(sys.argv)
    configure_fonts(app)
    window=Workbench();window.task.setCurrentText(task)
    if project:window.open_project_path(project)
    window.showMaximized();app.processEvents();window.canvas.fitInView(window.canvas.sceneRect(),Qt.KeepAspectRatio)
    return app.exec()
