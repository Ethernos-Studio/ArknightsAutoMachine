# qt - Qt6 实现

## 目录说明

本目录包含基于 PyQt6 的跨平台 GUI 实现，是 AAM 的主要 GUI 方案。

## 技术栈

- **PyQt6**: Qt6 Python 绑定
- **Qt6 Widgets**: 桌面 UI
- **Qt6 Core**: 信号槽、事件循环
- **cv2 (OpenCV)**: 截屏图像处理

## 目录结构

```
gui/qt/
├── src/
│   ├── main_window.py        # 主窗口实现
│   ├── map_view.py           # 截屏地图视图
│   ├── operator_palette.py   # 干员面板
│   └── qt_event_bridge.py    # Qt 信号槽 ↔ AAM 事件桥接
├── resources/
│   ├── qml/                  # QML 组件（可选）
│   └── icons/                # 图标资源
├── tests/
└── requirements.txt
```

## 主窗口

### 界面布局
```
┌─────────────────────────────────────┐
│  菜单栏  │  工具栏                                                      │
├─────────┴───────────────────────────┤
│  ┌─────────────┐       ┌───────────────┐ │
│  │                          │       │       控制面板               │ │
│  │   地图视图               │       │      ┌─────────┐  │ │
│  │                          │       │      │     状态信息     │  │ │
│  │                          │       │      └─────────┘  │ │
│  │                          │       │      ┌─────────┐  │ │
│  │                          │       │      │     干员栏       │  │ │
│  │                          │       │      └─────────┘  │ │
│  │                          │       │      ┌─────────┐  │ │
│  │                          │       │      │     日志面板     │  │ │
│  │                          │       │      └─────────┘  │ │
│  └─────────────┘       └───────────────┘ │
├─────────────────────────────────────┤
│  状态栏                                                                  │
└─────────────────────────────────────┘
```

### 代码示例
```python
# src/main_window.py
from PyQt6.QtWidgets import QMainWindow
from .map_view import MapView
from .operator_palette import OperatorPalette
from .log_panel import LogPanel

class MainWindow(QMainWindow):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.map_view = MapView(self)
        self.operator_palette = OperatorPalette(self)
        self.log_panel = LogPanel(self)
        self._setup_ui()
    
    def _setup_ui(self):
        # 设置界面布局
        pass
    
    def set_title(self, title: str):
        self.setWindowTitle(title)
    
    def map_canvas(self) -> 'MapView':
        return self.map_view
```

## 地图视图

### 截屏显示
```python
# src/map_view.py
from PyQt6.QtWidgets import QWidget
from PyQt6.QtGui import QPixmap, QPainter
from PyQt6.QtCore import Qt
import cv2
import numpy as np

class MapView(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.current_frame = QPixmap()
        self.setMinimumSize(640, 480)
    
    def render_frame(self, frame: np.ndarray):
        """渲染截屏帧"""
        # OpenCV BGR -> RGB
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        h, w, ch = rgb_frame.shape
        # 转换为 QPixmap
        from PyQt6.QtGui import QImage
        q_img = QImage(rgb_frame.data, w, h, ch * w, QImage.Format.Format_RGB888)
        self.current_frame = QPixmap.fromImage(q_img)
        self.update()
    
    def draw_detection(self, detection: dict):
        """绘制检测结果"""
        # 在帧上绘制检测框
        pass
    
    def paintEvent(self, event):
        painter = QPainter(self)
        if not self.current_frame.isNull():
            # 缩放以适应窗口
            scaled = self.current_frame.scaled(
                self.size(), 
                Qt.AspectRatioMode.KeepAspectRatio,
                Qt.TransformationMode.SmoothTransformation
            )
            # 居中绘制
            x = (self.width() - scaled.width()) // 2
            y = (self.height() - scaled.height()) // 2
            painter.drawPixmap(x, y, scaled)
```

## 事件桥接

```python
# src/qt_event_bridge.py
from PyQt6.QtCore import QObject, pyqtSignal, pyqtSlot
from typing import Optional
import numpy as np

class QtEventBridge(QObject):
    # 信号定义
    operator_selected = pyqtSignal(str)
    tile_clicked = pyqtSignal(int, int)  # x, y
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self._event_bus = None
    
    def bind_to_core(self, event_bus):
        """绑定到 AAM 核心事件总线"""
        self._event_bus = event_bus
        # 订阅核心事件
        event_bus.subscribe('frame_received', self._on_frame_received)
        event_bus.subscribe('state_changed', self._on_state_changed)
    
    @pyqtSlot(np.ndarray)
    def _on_frame_received(self, frame: np.ndarray):
        """处理帧接收事件"""
        pass
    
    @pyqtSlot(dict)
    def _on_state_changed(self, state: dict):
        """处理状态变化事件"""
        pass
```

## 运行

```bash
# 安装依赖
pip install -r requirements.txt

# 运行
python -m gui.qt.main
```

## 主题支持

```python
# 深色模式
from PyQt6.QtWidgets import QApplication, QStyleFactory
from PyQt6.QtGui import QPalette, QColor

app = QApplication([])
app.setStyle(QStyleFactory.create("Fusion"))

dark_palette = QPalette()
dark_palette.setColor(QPalette.ColorRole.Window, QColor(53, 53, 53))
dark_palette.setColor(QPalette.ColorRole.WindowText, QColor(255, 255, 255))
dark_palette.setColor(QPalette.ColorRole.Base, QColor(25, 25, 25))
dark_palette.setColor(QPalette.ColorRole.AlternateBase, QColor(53, 53, 53))
dark_palette.setColor(QPalette.ColorRole.ToolTipBase, QColor(255, 255, 255))
dark_palette.setColor(QPalette.ColorRole.ToolTipText, QColor(255, 255, 255))
dark_palette.setColor(QPalette.ColorRole.Text, QColor(255, 255, 255))
dark_palette.setColor(QPalette.ColorRole.Button, QColor(53, 53, 53))
dark_palette.setColor(QPalette.ColorRole.ButtonText, QColor(255, 255, 255))
dark_palette.setColor(QPalette.ColorRole.BrightText, QColor(255, 0, 0))
dark_palette.setColor(QPalette.ColorRole.Link, QColor(42, 130, 218))
dark_palette.setColor(QPalette.ColorRole.Highlight, QColor(42, 130, 218))
dark_palette.setColor(QPalette.ColorRole.HighlightedText, QColor(0, 0, 0))

app.setPalette(dark_palette)
```

## 相关目录

- [gui/abstract/](../abstract/): 抽象接口
- [gui/wpf/](../wpf/): WPF 实现