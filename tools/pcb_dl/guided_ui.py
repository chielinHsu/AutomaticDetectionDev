"""Five task pages with progressive disclosure."""
from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QCheckBox,QComboBox,QDoubleSpinBox,QFormLayout,QFrame,QGroupBox,QHBoxLayout,
    QHeaderView,QLabel,QLineEdit,QPlainTextEdit,QProgressBar,QPushButton,QScrollArea,QSpinBox,QStackedWidget,
    QTableWidget,QVBoxLayout,QWidget)
from .workbench_ui import build_shell,build_annotation_panel


class TaskPages(QStackedWidget):
    def sizeHint(self):
        return self.currentWidget().sizeHint() if self.currentWidget() else super().sizeHint()

    def minimumSizeHint(self):
        return self.currentWidget().minimumSizeHint() if self.currentWidget() else super().minimumSizeHint()


def text(value):
    label=QLabel(value);label.setTextFormat(Qt.PlainText);label.setWordWrap(True);return label


def button(w,layout,title,callback,primary=False):
    item=QPushButton(title);item.setMinimumHeight(34);item.setProperty('primary',primary)
    item.clicked.connect(w.safe(callback));layout.addWidget(item);return item


def advanced(layout,title):
    box=QGroupBox(title);box.setCheckable(True);box.setChecked(False)
    outer=QVBoxLayout(box);content=QWidget();form=QFormLayout(content);outer.addWidget(content);content.hide()
    box.toggled.connect(content.setVisible);layout.addWidget(box);return box,form


def build_ui(w):
    side,right_layout=build_shell(w)
    w.pages=TaskPages();side.addWidget(w.pages);page_layouts=[]
    for _ in range(5):
        page=QWidget();page_layouts.append(QVBoxLayout(page));w.pages.addWidget(page)
    a,b,c,d,e=page_layouts
    a.addWidget(text('載入圖片，或接續之前的工作'))
    w.add_image_button=button(w,a,'＋ 載入元件影像',w.add_image,True)
    button(w,a,'開啟標註專案（繼續教元件）',w.open_project)
    button(w,a,'開啟 CAD 配方（繼續編輯框）',w.open_recipe)
    a.addWidget(text('同一實物的不同顏色通道或 Z 拍攝屬於同一來源。標註時優先使用合併 RGB 圖。'))
    button(w,a,'我已有模型，直接產生 CAD',w.model_shortcut)
    a.addStretch();w.start_annotation=button(w,a,'開始標註元件 →',lambda:w.go_step(1),True)
    build_annotation_panel(w,b)
    w.spec_status=text('');c.addWidget(w.spec_status)
    c.addWidget(text('同類元件共用一組框尺寸。W 沿長軸，H 沿短軸；單位是原圖像素。'))
    w.profiles=QTableWidget(0,4);w.profiles.setHorizontalHeaderLabels(['元件類別','寬 W (px)','高 H (px)','子窗比例'])
    w.profiles.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch);w.profiles.setMaximumHeight(240);c.addWidget(w.profiles)
    w.measure_button=button(w,c,'從目前選取的元件帶入尺寸',w.measure_profile,True)
    c.addWidget(text('先在圖片點選一個已標註元件，再帶入尺寸並檢查。子窗比例 0.6 代表內框為主框的 60%。'))
    c.addStretch();w.data_status=text('');w.data_status.setObjectName('notice');d.addWidget(w.data_status)
    grouping=QGroupBox('準備資料：同一實物照片必須使用相同編號');gf=QFormLayout(grouping)
    w.group=QLineEdit();gf.addRow('目前圖片的實物／批次編號',w.group)
    w.split=QComboBox();w.split.addItems(['train','val','test']);w.split.hide();w.split_choice=QComboBox()
    for name,value in [('訓練：讓模型練習','train'),('驗證：調整設定','val'),('測試：最後獨立考試','test')]:w.split_choice.addItem(name,value)
    gf.addRow('這張圖片用於',w.split_choice);d.addWidget(grouping)
    d.addWidget(text('需要不同一實物子／獨立批次分作訓練、驗證與測試。不要把同一實物 RGB 或 Z 視圖拆開湊數。'))
    w.group.editingFinished.connect(w.sync_entry);w.split.currentTextChanged.connect(w.sync_entry)
    w.split_choice.currentIndexChanged.connect(lambda i:w.split.setCurrentText(w.split_choice.itemData(i)))
    w.split.currentTextChanged.connect(lambda value:w.split_choice.setCurrentIndex(w.split_choice.findData(value)))
    w.export_button=button(w,d,'1. 匯出已審核的訓練資料',w.export_data,True)
    button(w,d,'選擇之前匯出的資料',w.select_dataset);w.dataset_label=text('尚未匯出資料');d.addWidget(w.dataset_label)
    d.addWidget(text('2. 選擇學習方式'));w.task=QComboBox();w.task.addItems(['obb','segment']);w.task.hide()
    w.method=QComboBox();w.method.addItems(['旋轉框偵測（OBB）：位置與軸向','實例分割：輪廓、位置與軸向']);d.addWidget(w.method)
    w.method.currentIndexChanged.connect(lambda i:w.task.setCurrentIndex(i));w.task.currentIndexChanged.connect(w.method.setCurrentIndex)
    w.train_advanced,tf=advanced(d,'進階訓練設定（可先用預設）')
    w.epochs=QSpinBox();w.epochs.setRange(1,10000);w.epochs.setValue(100);tf.addRow('訓練回合數',w.epochs)
    w.imgsz=QSpinBox();w.imgsz.setRange(64,2048);w.imgsz.setSingleStep(32);w.imgsz.setValue(640);tf.addRow('模型輸入邊長 (px)',w.imgsz)
    w.train_button=button(w,d,'3. 開始訓練並建立模型',w.train,True)
    w.training_status=text('訓練在背景執行，完成後會顯示模型位置。');d.addWidget(w.training_status)
    w.model_label=text('尚未載入模型。已有模型可直接載入；還沒有則回到第 4 步訓練。');e.addWidget(w.model_label)
    button(w,e,'載入訓練完成的模型（model.json）',w.load_model,True)
    w.infer_advanced,inf=advanced(e,'進階辨識設定（可先用預設）')
    w.confidence=QDoubleSpinBox();w.confidence.setRange(.05,1);w.confidence.setValue(.5);w.confidence.setSingleStep(.05);inf.addRow('候選信心門檻',w.confidence)
    w.tile=QSpinBox();w.tile.setRange(64,4096);w.tile.setValue(1024);inf.addRow('分塊邊長 (px)',w.tile)
    w.generate_button=button(w,e,'辨識整張圖片，產生 CAD 框',w.infer_whole,True)
    w.roi_button=button(w,e,'只辨識我圈選的區域',lambda:w.choose_tool('roi'))
    button(w,e,'手動補一個 CAD 框',lambda:w.choose_tool('cad-rect'))
    w.view=QComboBox();w.view.addItems(['人工標註','CAD 配方']);w.view.hide();w.view.currentIndexChanged.connect(w.on_view_changed)
    w.edit_box=QGroupBox('調整目前選取的 CAD');edit=QFormLayout(w.edit_box);w.fields={}
    for key,name in [('x','中心 X (px)'),('y','中心 Y (px)'),('w','寬度 (px)'),('h','高度 (px)'),('angle','逆時針角度 (°)')]:
        spin=QDoubleSpinBox();spin.setRange(-100000,100000);spin.setDecimals(2);edit.addRow(name,spin);w.fields[key]=spin
    apply=QPushButton('套用單框修改');apply.clicked.connect(w.safe(w.apply_geometry));edit.addRow(apply);e.addWidget(w.edit_box)
    w.save_recipe_button=button(w,e,'儲存 CAD 配方',w.save_recipe,True)
    side.addStretch();w.progress=QProgressBar();w.progress.hide();side.addWidget(w.progress)
    w.cancel_button=button(w,side,'取消目前工作',w.cancel);w.cancel_button.hide()
    w.log_box,logform=advanced(side,'工作紀錄與詳細訊息');w.log=QPlainTextEdit();w.log.setReadOnly(True);w.log.setMaximumBlockCount(2000);w.log.setMaximumHeight(140);logform.addRow(w.log)
