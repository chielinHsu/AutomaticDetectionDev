# Component dual learning modes

## 修改紀錄

| 日期 | 修改者 | 說明 |
|---|---|---|
| 2026-10-30 | Codex (gpt-6.1-sol) | 通用元件用語、三欄雙主題實作與 Qt Creator 聯合建置。 |

<!--STE-->
## Purpose
The workbench trains two models for components. The OBB model locates rotated objects. The segmentation model traces each object.

## Reason
Appearance varies with light and component type. Manual annotations define the required classes. Separate source groups prevent data leaks across dataset splits.

## Effect
The model supplies class and pose. A CAD profile supplies frame dimensions. Users edit the results and save a recipe.

## Dependent inspection frames
The component frame supplies the location. The body frame defines the central inspection region. Each pin frame uses body coordinates.
Window frames use coordinates from their parent frames. Moving or rotating a parent moves its children. Frame sizes remain fixed in pixels.
Users teach these frames in task three. Each package class shares one frame group. Inference places the complete group on each component.
The recipe stores parent identifiers and all frame positions. Older profiles retain their ratio window. Pin identifiers do not identify physical polarity.

## Guided interface
Five task pages show the current goal and next action. Advanced settings stay collapsed. Frame widths remain constant during zoom.
Class groups stay collapsed until needed. Search selects matching names, classes, or numbers. Rectangle handles preserve right angles. Users can select polygon editing.
The workbench starts with a dark theme. Users can select a light theme. Theme changes preserve annotations and the current view.
The image list, canvas, and object inspector use separate columns. The inspector shows the selected object and its image crop.

## Joint build
Qt Creator builds both executable targets from one project. The workbench target checks the local Python environment and compiles its application files.
The launcher starts a separate Python process. The build does not download packages. Custom classes support other types of components.

## Procedure
1. Annotate each image.
2. Review the complete image.
3. Set source groups and dataset splits.
4. Set a CAD profile for each class.
5. Export both datasets.
6. Train and evaluate each model.
7. Load the model manifest.
8. Review the generated CAD objects.
9. Save the recipe.
<!--/STE-->

繁中：兩種模型共用人工輪廓與分組資料；各自訓練、評估及匯出 ONNX。物件觀察與 CAD 規格分離。工作台可編輯旋轉框及子窗，另存專用配方。沒有使用者標註時不能宣稱實際 PCB 模型準確度。

操作與格式：[README](../../../tools/pcb_dl/README.md)。決策：[ADR-037](../../adr/037-pcb-learning-workbench.md)。
