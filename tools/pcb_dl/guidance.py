"""Task guidance, prerequisite feedback and document safety for the workbench."""
import json
from pathlib import Path
from PySide6.QtCore import QProcess,Qt
from PySide6.QtWidgets import QFileDialog,QMessageBox,QTableWidgetItem
from .canvas_items import class_name
from .model import corners,inside,pose_from_polygon,validate_profiles,validate_project
from .annotation_geometry import annotation_shape
from .inspection import fits_inspection

TITLES=['1 · 載入元件影像','2 · 標註要辨識的元件','3 · 確認每類元件的檢測框','4 · 準備資料並訓練模型','5 · 產生 CAD，檢查後儲存']
HELP=['第一次使用從圖片開始；已有標註或配方可直接接續。','先選元件類別，再框住元件本體。畫完會出現帶編號的類別色框。',
      '把模型觀察到的元件位置，對應到你要使用的固定檢測框規格。','完成整圖標註與來源分組後，再建立能辨識這些類別的模型。','載入已訓練模型後，辨識整圖或選區；人工修正後再儲存配方。']


class Guidance:
    def signature(self,recipe=False):
        value={'image':self.entry().get('path'),'profiles':self.project['profiles'],'objects':self.objects} if recipe else self.project
        return json.dumps(value,sort_keys=True,ensure_ascii=False)

    def setup_guidance(self):
        self.step=0;self._ready=False;self.project_file=None;self.recipe_file=None;self.last_notice=''
        self.list_visible=True
        self.saved_project=self.signature();self.saved_recipe=self.signature(True)

    def show_notice(self,message,error=False):
        self.last_notice=message;self.notice.setText(('需要處理：' if error else '')+message)
        self.notice.setProperty('error',error);self.notice.style().unpolish(self.notice);self.notice.style().polish(self.notice)

    def go_step(self,index):
        if not self._ready:return
        for i,b in enumerate(self.step_buttons):b.setChecked(i==self.step)
        if self.canvas.points or self.canvas.start is not None:
            self.show_notice('繪製尚未完成；請先按 Enter 完成輪廓，或 Esc 取消，再切換步驟。');return
        if self.worker is not None and self.worker.isRunning():
            self.show_notice('正在辨識；請等待完成或按「取消目前工作」。');return
        self.step=max(0,min(4,index));self.pages.setCurrentIndex(self.step)
        for i,b in enumerate(self.step_buttons):b.setChecked(i==self.step)
        self.step_title.setText(TITLES[self.step]);self.step_help.setText(HELP[self.step])
        self.view.blockSignals(True);self.view.setCurrentIndex(1 if self.step==4 else 0);self.view.blockSignals(False)
        self.choose_tool('select');self.refresh()
        self.show_notice(self.step_message())

    def step_message(self):
        if self.image is None:return '先按「載入元件影像」，或「開啟標註專案」。'
        if self.step==1:return '選類別 → 按「開始畫矩形框」→ 在圖片上按住左鍵拖曳，放開就完成一個標註。'
        if self.step==2:return self.profile_issue() or '框規格已填寫；可前往第 4 步準備訓練資料。'
        if self.step==3:return self.data_issue() or '標註與分組已完成，可以匯出資料。'
        if self.step==4:return self.model_issue() or '模型已載入。按「辨識整張圖片」或圈選要辨識的區域。'
        return '圖片已載入。按「開始標註元件」繼續。'

    def next_step(self):
        if self.step==4:return self.save_recipe()
        self.go_step(self.step+1)

    def on_view_changed(self,index):
        if self._ready and index==1 and self.step!=4:self.go_step(4)
        else:self.refresh()

    def choose_tool(self,mode):
        if self.image is None and mode!='select':self.show_notice('請先載入一張圖片。',True);self.tool_changed(self.canvas.mode);return
        if mode!=self.canvas.mode and (self.canvas.points or self.canvas.start is not None):
            self.show_notice('請先完成輪廓或按 Esc 取消，再切換工具。');self.tool_changed(self.canvas.mode);return
        self.canvas.set_mode(mode)
        if mode!='select':self.canvas.setFocus()
        if mode=='annotation-poly':self.show_notice('輪廓草稿：逐點放置頂點，至少三點後按「完成輪廓」、Enter 或起點閉合；Backspace 退一點，Esc 取消。')

    def tool_changed(self,mode):
        names={'select':'選取／編輯：拖框移動、拖白色頂點調整；空白處可平移影像。Delete 刪除，Ctrl+Z 復原。','pan':'平移檢視：從任何位置拖動影像，包含元件上方；標註位置保持不變。','annotation-rect':'畫矩形：按住左鍵拖曳，放開完成；Esc 取消。',
               'annotation-poly':f'輪廓草稿：已放 {len(self.canvas.points)} 點（至少 3 點）。Enter／完成輪廓或點綠色起點閉合；Backspace 退一點，Esc 取消。','roi':'圈選辨識範圍：拖出區域後放開。','cad-rect':'手動補 CAD：先選類別，再拖出元件位置。',
               'inspection-body':'拖出中央本體檢測框；導腳跟隨本體，Esc 取消。','inspection-pin':'拖出一隻導腳檢測框，相對本體保存；Esc 取消。',
               'inspection-window':'拖出相依檢測窗，相對清單中選定的父框保存；Esc 取消。'}
        self.tool_hint.setText('目前工具 · '+names.get(mode,mode))
        for key,b in self.tool_buttons.items():b.setChecked(key==mode)
        self.finish_contour_button.setVisible(mode=='annotation-poly')
        self.finish_contour_button.setEnabled(mode=='annotation-poly' and len(self.canvas.points)>=3 and self.canvas.isEnabled())
        pending=bool(self.canvas.points or self.canvas.start is not None)
        self.cancel_draft_button.setVisible(mode in ('annotation-poly','annotation-rect','roi','cad-rect') or mode.startswith('inspection-'))
        self.cancel_draft_button.setEnabled(pending)
        if hasattr(self,'draw_button'):self.draw_button.setText('正在畫矩形：到圖片上按住左鍵拖曳' if mode=='annotation-rect' else '開始畫矩形框（按住滑鼠拖曳）')
        if self._ready:self.update_canvas_status();self.update_draft_context()

    def draft_changed(self,count):
        self.tool_changed(self.canvas.mode)
        if self._ready:self.update_guidance()

    def update_draft_context(self):
        if self.canvas.mode=='annotation-poly' and (self.canvas.points or self.selected_index<0):
            self.selection_label.setText(f'輪廓草稿：已放置 {len(self.canvas.points)} 個點，至少需要 3 點。按畫布上方「完成輪廓」後，才會建立標註並顯示白色編輯把手。')
            self.draw_button.hide()

    def update_save_status(self):
        pending=bool(self.canvas.points or self.canvas.start is not None)
        dirty=self.signature()!=self.saved_project or (bool(self.objects) and self.signature(True)!=self.saved_recipe)
        state='草稿尚未完成（尚未存入標註）' if pending else '尚未儲存變更' if dirty else '已儲存／無新變更'
        self.save_status.setText(state+(' · '+Path(self.project_file).name if self.project_file else ''))
        self.reviewed.setEnabled(self.image is not None and not pending)

    def profile_values(self):
        profiles={}
        for row,name in enumerate(self.project['classes']):
            values=[self.profiles.item(row,c).text().strip() for c in (1,2,3)]
            if not values[0] or not values[1]:raise ValueError(f'請設定「{class_name(name)}」的寬度與高度，或從選取的標註帶入。')
            profiles[name]={**self.project['profiles'].get(name,{}),**dict(zip(['w','h','window_ratio'],map(float,values)))}
        validate_profiles(profiles,self.project['classes']);return profiles

    def profile_issue(self):
        try:self.profile_values();return ''
        except (ValueError,AttributeError,TypeError):return '請填完整、有效的檢測框尺寸；可以先選一個標註，按「帶入尺寸」。'

    def data_issue(self):
        images=self.project['images']
        if not images:return '還沒有圖片，請先完成第 1 步。'
        missing=sum(not e.get('reviewed') for e in images)
        if missing:return f'還有 {missing} 張圖片尚未完成標註檢查。完成後在第 2 步勾選「已標註並檢查」。'
        groups={}
        for entry in images:
            group=entry.get('group','').strip();split=entry['split']
            if not group:return '請設定每張圖的實物／批次編號。'
            if group in groups and groups[group]!=split:return '同一實物／批次不能分到不同用途，請修正分組。'
            groups[group]=split
        if len(groups)<3:return f'目前只有 {len(groups)} 個獨立來源；還需要其他實物／批次供驗證與測試，不能拆同一實物 Z 或 RGB 湊數。'
        if set(groups.values())!={'train','val','test'}:return '請分別指定訓練、驗證、測試用途，每類至少一個獨立來源。'
        return self.profile_issue()

    def model_issue(self):
        if not self.manifest:return '尚未載入模型；先按「載入訓練完成的模型」，或回第 4 步訓練。'
        if not Path(self.manifest).is_file():return '找不到模型檔案，請重新載入 model.json。'
        if getattr(self,'manifest_task',self.task.currentText())!=self.task.currentText():return '目前方式與已載入模型不同，請載入對應方式的模型。'
        if self.image is None:return '模型已選取，請先載入要辨識的 元件影像。'
        return self.profile_issue()

    def update_guidance(self,*_):
        if not self._ready:return
        has=self.image is not None;busy=self.worker is not None and self.worker.isRunning()
        training=self.process is not None and self.process.state()!=QProcess.NotRunning
        self.image_stack.setCurrentIndex(0 if has else 1);self.image_count.setText(f'{self.current+1 if has else 0} / {len(self.project["images"])} 張')
        self.start_annotation.setEnabled(has);self.draw_button.setEnabled(has and not busy)
        for tool in self.tool_buttons.values():tool.setEnabled(has and not busy)
        self.object_panel.setVisible(self.step in (1,2,4) and self.list_visible);self.object_toggle.setVisible(self.step in (1,2,4))
        self.object_toggle.setText('隱藏元件清單' if self.list_visible else '顯示元件清單');self.edit_box.setVisible(self.step==4 and bool(self.objects))
        self.previous_button.setEnabled(self.step>0 and not busy);self.next_button.setEnabled(has and not busy)
        self.next_button.setVisible(self.step<4);self.next_button.setText(['下一步：標註 →','下一步：框規格 →','下一步：訓練 →','下一步：產生 CAD →','儲存配方'][self.step])
        selected=self.selected_index;labels=self.entry().get('annotations',[])
        selected_valid=self.view.currentIndex()==0 and 0<=selected<len(labels)
        self.reclassify_button.setEnabled(selected_valid and not busy);self.measure_button.setEnabled(selected_valid)
        self.draw_button.setVisible(not selected_valid)
        shape=annotation_shape(labels[selected]) if selected_valid else None
        hint='矩形：拖角點保持直角，可各自調整長寬。' if shape=='rectangle' else '輪廓已完成：拖白色頂點調整，點位太密時先放大。'
        self.selection_label.setText(f'已選取 #{selected+1} · {class_name(labels[selected]["class"])}；'+hint if selected_valid else '③ 點一個框，可拖移、調整形狀或修改類別。')
        self.shape_rect_button.setEnabled(selected_valid and not busy);self.shape_poly_button.setEnabled(selected_valid and not busy)
        self.shape_rect_button.setChecked(shape=='rectangle');self.shape_poly_button.setChecked(shape=='polygon')
        self.spec_status.setText(self.profile_issue() or '每個類別的檢測框規格已設定。')
        ready=sum(e.get('reviewed',False) for e in self.project['images']);groups=len({e['group'] for e in self.project['images']})
        self.data_status.setText(f'圖片檢查 {ready}/{len(self.project["images"])} · 獨立來源 {groups} 個。 '+self.data_issue())
        self.export_button.setEnabled(not self.data_issue() and not busy and not training)
        dataset=getattr(self,'dataset_path','');self.dataset_label.setText('已匯出／選取：'+str(dataset) if dataset else '尚未準備訓練資料。先完成標註，再按上方匯出。')
        self.train_button.setEnabled(bool(dataset) and not busy and not training)
        self.generate_button.setEnabled(not self.model_issue() and not busy);self.roi_button.setEnabled(self.generate_button.isEnabled())
        self.save_recipe_button.setEnabled(bool(self.objects) and not busy);self.cancel_button.setVisible(busy or training)
        self.progress.setVisible(busy or training)
        if training:self.progress.setRange(0,0)
        self.update_save_status()
        self.update_browser_selection()
        self.update_workbench()
        self.update_inspection_editor()
        self.update_draft_context()

    def open_project_path(self,path):
        self.idle();project=json.loads(Path(path).read_text(encoding='utf-8-sig'));validate_project(project)
        self.project=project;self.current=-1;self.profile_table()
        index=next((i for i,e in enumerate(project['images']) if e['annotations']),0)
        self.reload_images(index);self.mark_saved(str(path));self.go_step(1)

    def clear_selection(self):
        self.selected_index=-1;self.table.blockSignals(True);self.table.clearSelection();self.table.setCurrentCell(-1,-1);self.table.blockSignals(False);self.update_guidance()

    def annotation_added(self):
        self.selected_index=len(self.entry()['annotations'])-1
        if self.step!=1:self.go_step(1)
        self.select(self.selected_index);self.canvas.choose(self.selected_index)
        label=self.entry()['annotations'][-1]
        message='輪廓已完成；現在可拖白色頂點調整，Ctrl+Z 可復原。' if annotation_shape(label)=='polygon' else '類別色框已畫好；選錯類別可在右側修改。'
        self.show_notice(f'已新增 #{self.selected_index+1} · {class_name(label["class"])}。'+message)

    def reclassify(self):
        self.require_image();labels=self.entry()['annotations'];index=self.selected_index
        if not 0<=index<len(labels):return
        h,w=self.image.shape[:2]
        if not fits_inspection(pose_from_polygon(labels[index]['points']),self.project['profiles'].get(self.kind.currentData(),{}),(0,0,w,h)):
            raise ValueError('新類別的相依檢測框超出影像，原類別保留。')
        self.remember();labels[index]['class']=self.kind.currentData();self.invalidate_review();self.refresh()
        self.show_notice(f'已將 #{index+1} 改為 {class_name(labels[index]["class"])}。')

    def convert_annotation_shape(self,shape):
        self.require_image();labels=self.entry()['annotations'];index=self.selected_index
        if self.view.currentIndex()!=0 or not 0<=index<len(labels):return
        label=labels[index]
        if shape not in ('rectangle','polygon'):raise ValueError('不支援的形狀類型')
        self.choose_tool('select')
        if annotation_shape(label)==shape:
            self.show_notice('目前已是矩形拉伸模式。' if shape=='rectangle' else '目前已是多邊形頂點模式。');return
        points=corners(pose_from_polygon(label['points'])) if shape=='rectangle' else label['points']
        h,w=self.image.shape[:2]
        if not inside(points,(0,0,w,h)):raise ValueError('轉換後的矩形超出圖片，請先調整輪廓位置。')
        if not fits_inspection(pose_from_polygon(points),self.project['profiles'].get(label['class'],{}),(0,0,w,h)):
            raise ValueError('轉換後的相依檢測框超出影像，原輪廓保留。')
        self.remember();label['points']=points;label['shape']=shape;self.invalidate_review();self.refresh()
        self.show_notice('已轉為矩形，拖動角點會保持直角；Ctrl+Z 可復原。' if shape=='rectangle' else '已轉為多邊形，現在可自由調整頂點；Ctrl+Z 可復原。')

    def measure_profile(self):
        self.require_image();labels=self.entry()['annotations'];index=self.selected_index
        if not 0<=index<len(labels):return
        label=labels[index];size=pose_from_polygon(label['points']);row=self.project['classes'].index(label['class'])
        self.profiles.setItem(row,1,QTableWidgetItem(f'{size["w"]:.2f}'));self.profiles.setItem(row,2,QTableWidgetItem(f'{size["h"]:.2f}'))
        self.show_notice(f'已帶入 {class_name(label["class"])}：{size["w"]:.1f} × {size["h"]:.1f} px。請確認這是要使用的框規格。');self.update_guidance()

    def model_shortcut(self):self.go_step(4);self.load_model()

    def select_dataset(self):
        directory=QFileDialog.getExistingDirectory(self,'選擇之前匯出的訓練資料')
        if not directory:return
        if not (Path(directory)/'dataset-files.json').is_file():raise ValueError('此資料夾不是工作台匯出的資料。')
        self.dataset_path=directory;self.update_guidance();self.show_notice('已選取訓練資料；按「開始訓練」繼續。')

    def mark_saved(self,path,recipe=False):
        if recipe:self.recipe_file=path;self.saved_recipe=self.signature(True)
        else:self.project_file=path;self.saved_project=self.signature()
        self.show_notice('已儲存：'+str(path));self.update_guidance()

    def inference_progress(self,done,total):
        self.progress.setRange(0,total);self.progress.setValue(done)
        self.show_notice(f'正在辨識圖片：已完成 {done} / {total} 個區塊。')

    def inference_failed(self,message):
        self.inference_error=message;self.log.appendPlainText('辨識未完成：'+message);self.show_notice(message,True)

    def inference_finished(self):
        self.canvas.setEnabled(True);self.update_guidance()
        if getattr(self,'inference_error',''):
            self.show_notice(self.inference_error,True);return
        self.go_step(4);self.show_notice(f'辨識完成，目前有 {len(self.objects)} 個 CAD。請檢查位置及類別，再按「儲存 CAD 配方」。' if self.objects else '沒有找到元件。請確認模型適用這張圖及元件類別，或檢查進階門檻設定。')

    def training_finished(self,code,output):
        manifest=Path(output)/self.task.currentText()/'deployment/model.json'
        message='模型已建立，可在第 5 步載入：'+str(manifest) if code==0 and manifest.is_file() else f'訓練未完成（exit {code}），請展開工作紀錄查看原因。'
        self.training_status.setText(message);self.log.appendPlainText(message);self.update_guidance();self.show_notice(message,code!=0)

    def save_current(self):
        return self.save_recipe() if self.view.currentIndex()==1 else self.save_project()

    def close_workbench(self,event):
        if self.worker is not None and self.worker.isRunning():self.cancel();self.show_notice('正在取消，待目前分塊完成後再關閉。');event.ignore();return
        if self.process is not None and self.process.state()!=QProcess.NotRunning:
            self.show_notice('訓練尚未完成，請先完成或取消工作。');event.ignore();return
        try:self.idle()
        except ValueError as exc:
            self.show_notice(str(exc),True);self.canvas.setFocus();event.ignore();return
        self.sync_entry();project_dirty=self.signature()!=self.saved_project
        recipe_dirty=bool(self.objects) and self.signature(True)!=self.saved_recipe
        if project_dirty or recipe_dirty:
            answer=QMessageBox.question(self,'儲存後再離開？','尚有未儲存的標註或配方。',QMessageBox.Save|QMessageBox.Discard|QMessageBox.Cancel,QMessageBox.Save)
            if answer==QMessageBox.Cancel:event.ignore();return
            if answer==QMessageBox.Save:
                try:
                    if project_dirty:self.save_project()
                    if recipe_dirty:self.save_recipe()
                except Exception as exc:self.show_notice(str(exc),True);event.ignore();return
                if self.signature()!=self.saved_project or (recipe_dirty and self.signature(True)!=self.saved_recipe):event.ignore();return
        event.accept()
