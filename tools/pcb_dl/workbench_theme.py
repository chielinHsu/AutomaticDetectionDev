"""Window-local themes; appearance changes never rebuild the document or canvas."""
from PySide6.QtCore import QSettings
from PySide6.QtGui import QColor,QPalette

COLORS={
    'dark':dict(bg='#0B1220',panel='#121D2D',field='#172538',ink='#E8EEF7',muted='#A9B8CD',border='#64748B',note='#182D49',selection='#1E3A64'),
    'light':dict(bg='#F1F5F9',panel='#FFFFFF',field='#F8FAFC',ink='#172B43',muted='#52647B',border='#7B8CA3',note='#EAF2FF',selection='#DBEAFE')}


def theme_settings():return QSettings('PCBVisionStudio','Workbench')


def apply_theme(window,mode=None,persist=False):
    mode=mode or str(window.settings.value('appearance/theme','dark'))
    if mode not in COLORS:mode='dark'
    t=COLORS[mode];window.theme_mode=mode
    palette=QPalette()
    for role,key in [(QPalette.Window,'bg'),(QPalette.WindowText,'ink'),(QPalette.Base,'field'),
                     (QPalette.AlternateBase,'panel'),(QPalette.Text,'ink'),(QPalette.Button,'panel'),
                     (QPalette.ButtonText,'ink'),(QPalette.ToolTipBase,'panel'),(QPalette.ToolTipText,'ink')]:
        palette.setColor(role,QColor(t[key]))
    palette.setColor(QPalette.Highlight,QColor('#2563EB'));palette.setColor(QPalette.HighlightedText,QColor('white'));window.setPalette(palette)
    window.setStyleSheet(f'''
        QMainWindow, QWidget#workspace {{ background:{t['bg']}; color:{t['ink']}; }}
        QWidget {{ color:{t['ink']}; }} QLabel {{ background:transparent; }}
        QLabel#brand {{ font-size:19px; font-weight:700; }}
        QLabel#stepTitle {{ font-size:17px; font-weight:600; }}
        QLabel#muted, QLabel#canvasStatus {{ color:{t['muted']}; }}
        QFrame#panel, QWidget#inspector {{ background:{t['panel']}; border:1px solid {t['border']}; border-radius:6px; }}
        QLabel#notice {{ background:{t['note']}; color:{t['ink']}; border-left:3px solid #3B82F6; padding:7px 10px; }}
        QLabel#notice[error="true"] {{ background:{'#492C22' if mode=='dark' else '#FFE9DF'}; color:{'#FFD9C9' if mode=='dark' else '#702A13'}; border-left-color:#F59E0B; }}
        QLabel#toolHint {{ color:{t['muted']}; padding:5px; }}
        QPushButton, QToolButton {{ padding:6px 9px; min-height:22px; border:1px solid {t['border']}; border-radius:5px; background:{t['panel']}; color:{t['ink']}; }}
        QPushButton:hover, QToolButton:hover {{ border-color:#3B82F6; background:{t['note']}; }}
        QPushButton:checked, QToolButton:checked, QPushButton[primary="true"] {{ background:#2563EB; color:white; border-color:#2563EB; font-weight:600; }}
        QPushButton:disabled, QToolButton:disabled {{ color:{t['muted']}; background:{t['field']}; border-color:{t['field']}; }}
        QPushButton:focus, QToolButton:focus, QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus {{ border:2px solid #3B82F6; }}
        QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox,QPlainTextEdit {{ min-height:26px; padding:3px 5px; background:{t['field']}; color:{t['ink']}; border:1px solid {t['border']}; border-radius:4px; selection-background-color:#2563EB; selection-color:white; }}
        QComboBox QAbstractItemView {{ background:{t['field']}; color:{t['ink']}; selection-background-color:#2563EB; selection-color:white; }}
        QGroupBox {{ margin-top:12px; padding:8px; border:1px solid {t['border']}; border-radius:5px; }}
        QGroupBox::title {{ subcontrol-origin:margin; left:8px; }}
        QTreeWidget,QListWidget,QTableWidget {{ background:{t['panel']}; alternate-background-color:{t['field']}; color:{t['ink']}; border:0; outline:0; gridline-color:{t['border']}; }}
        QTreeWidget::item, QListWidget::item {{ padding:5px 2px; }}
        QTreeWidget::item:selected, QListWidget::item:selected {{ background:{t['selection']}; color:{t['ink']}; border-left:3px solid #3B82F6; }}
        QHeaderView::section {{ padding:5px; background:{t['field']}; color:{t['muted']}; border:0; }}
        QScrollArea, QScrollArea > QWidget > QWidget {{ background:{t['panel']}; border:0; }}
        QSplitter::handle {{ background:{t['bg']}; }} QCheckBox {{ spacing:6px; padding:4px 0; }}
        QCheckBox::indicator {{ width:16px; height:16px; }}
        QStatusBar {{ background:{t['panel']}; color:{t['muted']}; }}
    ''')
    window.canvas.setBackgroundBrush(QColor('#080F19' if mode=='dark' else '#E2E8F0'))
    window.theme_choice.blockSignals(True);window.theme_choice.setCurrentIndex(0 if mode=='dark' else 1);window.theme_choice.blockSignals(False)
    if persist:window.settings.setValue('appearance/theme',mode);window.settings.sync()
