"""Workbench shell and annotation inspector; other task pages keep their workflow."""
from PySide6.QtCore import QSize,Qt
from PySide6.QtWidgets import (QCheckBox,QComboBox,QDoubleSpinBox,QFrame,QGridLayout,QHBoxLayout,
    QLabel,QLineEdit,QListWidget,QPushButton,QScrollArea,QSplitter,QStackedWidget,QVBoxLayout,QWidget)
from .object_browser import ObjectBrowser


def label(value='',name=None):
    item=QLabel(value);item.setTextFormat(Qt.PlainText);item.setWordWrap(True)
    if name:item.setObjectName(name)
    if name=='brand':item.setWordWrap(False);item.setMinimumWidth(195)
    return item


def action(w,layout,title,callback,primary=False):
    item=QPushButton(title);item.setProperty('primary',primary);item.clicked.connect(w.safe(callback));layout.addWidget(item);return item


def panel(title):
    box=QFrame();box.setObjectName('panel');layout=QVBoxLayout(box);layout.setContentsMargins(10,10,10,10);layout.setSpacing(7)
    caption=label(title);caption.setStyleSheet('font-weight:600;');layout.addWidget(caption)
    return box,layout


def build_shell(w):
    host=QWidget();host.setObjectName('workspace');root=QVBoxLayout(host);root.setContentsMargins(12,10,12,8);root.setSpacing(8)
    header=QHBoxLayout();root.addLayout(header);header.addWidget(label('Vision CAD Studio','brand'))
    w.save_status=label('尚未載入專案','muted');header.addWidget(w.save_status,1)
    w.global_actions=QHBoxLayout();header.addLayout(w.global_actions)
    w.theme_choice=QComboBox();w.theme_choice.addItems(['深色','淺色']);w.theme_choice.setToolTip('首次使用預設深色，之後記住選擇')
    w.theme_choice.currentIndexChanged.connect(lambda i:w.change_theme('dark' if i==0 else 'light'));header.addWidget(w.theme_choice)
    nav=QHBoxLayout();root.addLayout(nav);w.step_buttons=[]
    for index,name in enumerate(['1  載入圖片','2  標註元件','3  設定檢測框','4  訓練模型','5  產生與儲存']):
        b=QPushButton(name);b.setCheckable(True);b.setMinimumHeight(40);b.clicked.connect(lambda checked=False,i=index:w.go_step(i));nav.addWidget(b);w.step_buttons.append(b)
    w.step_title=label('','stepTitle');w.step_help=label('','muted');w.step_title.hide();w.step_help.hide()
    w.notice=label('','notice');root.addWidget(w.notice)
    w.splitter=QSplitter(Qt.Horizontal);w.splitter.setChildrenCollapsible(False);root.addWidget(w.splitter,1)
    left=QWidget();left.setMinimumWidth(235);left_layout=QVBoxLayout(left);left_layout.setContentsMargins(0,0,0,0);left_layout.setSpacing(8)
    w.left_panel=left;w.splitter.addWidget(left)
    box,images=panel('影像清單');left_layout.addWidget(box,2);action(w,images,'＋ 新增圖片',w.add_image)
    w.image_list=QListWidget();w.image_list.setIconSize(QSize(72,50));w.image_list.setMinimumHeight(70)
    w.image_list.currentRowChanged.connect(w.select_image_row);images.addWidget(w.image_list,1)
    w.images=QComboBox();w.images.hide();w.images.currentIndexChanged.connect(w.safe(w.switch_image));images.addWidget(w.images)
    box,classes=panel('新增標註類別');left_layout.addWidget(box)
    class_row=QHBoxLayout();classes.addLayout(class_row);w.kind=QComboBox();class_row.addWidget(w.kind,1)
    action(w,class_row,'＋',w.add_class).setToolTip('新增元件類別／封裝')
    w.class_counts=label('','muted');classes.addWidget(w.class_counts)
    w.object_panel,objects=panel('標註清單 · 依類別收合');left_layout.addWidget(w.object_panel,3)
    w.object_search=QLineEdit();w.object_search.setPlaceholderText('類別、名稱、#編號');w.object_search.setClearButtonEnabled(True)
    row=QHBoxLayout();objects.addLayout(row);row.addWidget(w.object_search,1);w.table=ObjectBrowser();w.table.setMinimumHeight(100)
    action(w,row,'+',w.table.expandAll).setFixedWidth(28);action(w,row,'-',w.table.collapseAll).setFixedWidth(28)
    w.search_status=label('','muted');w.search_status.hide();objects.addWidget(w.search_status);objects.addWidget(w.table,1)
    w.table.itemSelectionChanged.connect(w.table_selection);w.table.objectActivated.connect(w.focus_object)
    w.table.queryResetRequested.connect(w.object_search.clear);w.table.summaryChanged.connect(w.search_status.setText)
    w.table.visibleObjectsChanged.connect(w.canvas.set_visible_objects);w.object_search.textChanged.connect(w.table.filter_objects)
    center=QWidget();center.setMinimumWidth(300);center_layout=QVBoxLayout(center);center_layout.setContentsMargins(0,0,0,0);center_layout.setSpacing(6);w.splitter.addWidget(center)
    image_row=QHBoxLayout();center_layout.addLayout(image_row);w.image_title=label('尚未載圖');image_row.addWidget(w.image_title,1)
    w.image_previous=action(w,image_row,'‹',lambda:w.change_image(-1));w.image_count=label('0 / 0');image_row.addWidget(w.image_count)
    w.image_next=action(w,image_row,'›',lambda:w.change_image(1))
    w.zoom_choice=QComboBox();w.zoom_choice.addItems(['25%','50%','75%','100%','125%','150%','200%','400%']);w.zoom_choice.setMinimumWidth(75)
    w.zoom_choice.activated.connect(lambda i:w.canvas.set_zoom(float(w.zoom_choice.itemText(i).strip('%'))));image_row.addWidget(w.zoom_choice)
    action(w,image_row,'適配',w.canvas.fit_image);w.object_toggle=action(w,image_row,'清單',w.toggle_object_list)
    w.all_labels=QCheckBox('全部標籤');w.all_labels.toggled.connect(w.canvas.set_label_visibility);center_layout.addWidget(w.all_labels)
    board=QHBoxLayout();center_layout.addLayout(board,1);tools=QVBoxLayout();board.addLayout(tools);w.tool_buttons={}
    for title,mode,hint in [('選取','select','選取／拖移'),('平移','pan','平移影像'),('矩形','annotation-rect','畫矩形'),('輪廓','annotation-poly','畫多邊形')]:
        item=action(w,tools,title,lambda m=mode:w.choose_tool(m));item.setFixedWidth(48);item.setCheckable(True);item.setToolTip(hint);w.tool_buttons[mode]=item
    action(w,tools,'放大',lambda:w.canvas.zoom_by(1.2)).setFixedWidth(48);action(w,tools,'縮小',lambda:w.canvas.zoom_by(1/1.2)).setFixedWidth(48);tools.addStretch()
    w.image_stack=QStackedWidget();w.image_stack.addWidget(w.canvas);board.addWidget(w.image_stack,1)
    empty=QWidget();empty_layout=QVBoxLayout(empty);empty_layout.addStretch();empty_layout.addWidget(label('從第一張元件影像開始','stepTitle'))
    empty_layout.addWidget(label('載入圖片，選元件類別，再框住元件本體。'))
    action(w,empty_layout,'載入元件影像',w.add_image,True);action(w,empty_layout,'開啟已有標註專案',w.open_project);empty_layout.addStretch();w.image_stack.addWidget(empty)
    w.tool_hint=label('','toolHint');center_layout.addWidget(w.tool_hint)
    w.legend=label('類別色：電阻藍 · 電容橙 · IC 紫；選取加白色把手','muted');center_layout.addWidget(w.legend)
    w.canvas_status=label('','canvasStatus');center_layout.addWidget(w.canvas_status)
    right=QWidget();right.setObjectName('inspector');right.setMinimumWidth(300);w.splitter.addWidget(right)
    right_layout=QVBoxLayout(right);right_layout.setContentsMargins(10,10,10,10)
    title_row=QHBoxLayout();right_layout.addLayout(title_row);title_row.addWidget(label('屬性與目前任務','stepTitle'),1);w.delete_button=action(w,title_row,'刪除',w.delete)
    scroll=QScrollArea();scroll.setWidgetResizable(True);scroll.setFrameShape(QFrame.NoFrame);right_layout.addWidget(scroll,1)
    panel_widget=QWidget();side=QVBoxLayout(panel_widget);side.setContentsMargins(0,0,0,0);side.setSpacing(8);scroll.setWidget(panel_widget)
    w.name_panel=QWidget();name_layout=QVBoxLayout(w.name_panel);name_layout.setContentsMargins(0,0,0,0)
    w.name_title=label('選取項目名稱','stepTitle');name_layout.addWidget(w.name_title)
    w.object_name=QLineEdit();w.object_name.setMaxLength(128);w.object_name.setPlaceholderText('自訂名稱，例如 R22、U1');name_layout.addWidget(w.object_name)
    action(w,name_layout,'套用名稱',w.rename_selected);side.addWidget(w.name_panel)
    footer=QHBoxLayout();root.addLayout(footer);footer.addWidget(label('元件深度學習 CAD','muted'),1)
    w.previous_button=action(w,footer,'← 上一步',lambda:w.go_step(w.step-1));w.next_button=action(w,footer,'下一步 →',w.next_step,True)
    w.splitter.setSizes([260,850,355]);w.splitter.setStretchFactor(0,0);w.splitter.setStretchFactor(1,1);w.splitter.setStretchFactor(2,0)
    w._thumbnail_cache={};w.setCentralWidget(host)
    return side,right_layout


def build_annotation_panel(w,layout):
    w.selection_label=label('點選畫布上的元件以檢查屬性。');layout.addWidget(w.selection_label)
    w.draw_button=action(w,layout,'開始畫矩形框（按住滑鼠拖曳）',lambda:w.choose_tool('annotation-rect'))
    w.annotation_details=QWidget();details=QVBoxLayout(w.annotation_details);details.setContentsMargins(0,0,0,0);layout.addWidget(w.annotation_details)
    details.addWidget(label('元件類別'));w.selected_class=QComboBox();details.addWidget(w.selected_class)
    w.reclassify_button=action(w,details,'套用選取類別',w.reclassify_selected)
    row=QHBoxLayout();details.addLayout(row)
    w.shape_rect_button=action(w,row,'矩形拉伸',lambda:w.convert_annotation_shape('rectangle'));w.shape_rect_button.setCheckable(True)
    w.shape_poly_button=action(w,row,'多邊形頂點',lambda:w.convert_annotation_shape('polygon'));w.shape_poly_button.setCheckable(True)
    details.addWidget(label('幾何資訊 · 中心座標／原圖像素'));grid=QGridLayout();details.addLayout(grid);w.annotation_fields={}
    for i,(key,title) in enumerate([('x','中心 X (px)'),('y','中心 Y (px)'),('w','寬 W (px)'),('h','高 H (px)'),('angle','角度 (°)')]):
        spin=QDoubleSpinBox();spin.setRange(-1000000,1000000);spin.setDecimals(2);spin.setKeyboardTracking(False)
        cell=QVBoxLayout();cell.addWidget(label(title,'muted'));cell.addWidget(spin);grid.addLayout(cell,i//2,i%2);w.annotation_fields[key]=spin
    w.annotation_apply=action(w,details,'套用矩形幾何',w.apply_annotation_geometry)
    preview_row=QHBoxLayout();details.addLayout(preview_row);w.crop_preview=QLabel();w.crop_preview.setFixedSize(130,100);w.crop_preview.setAlignment(Qt.AlignCenter)
    preview_row.addWidget(w.crop_preview);w.geometry_status=label();preview_row.addWidget(w.geometry_status,1)
    w.reviewed=QCheckBox('這張圖的元件皆已標註並檢查');w.reviewed.setToolTip('幾何有效不代表整圖已標註完成；修改標註後會取消審核。');layout.addWidget(w.reviewed)
    w.reviewed.toggled.connect(w.sync_entry);action(w,layout,'儲存標註進度',w.save_project);layout.addStretch()
