# -*- coding: utf-8 -*-
"""
明日方舟游戏状态检测 CLI 工具

一个功能完整的命令行工具，用于检测游戏截图中的对局状态。

功能：
- 单张图像检测
- 批量图像处理
- 实时屏幕监控
- 配置文件支持
- 日志记录
- 结果导出（JSON/CSV）

用法：
    python cli.py detect --image screenshot.png
    python cli.py detect --batch ./screenshots/ --output results.json
    python cli.py monitor --interval 1.0 --duration 60
    python cli.py config --show
    python cli.py test

作者: Vision System
版本: 1.0.0
"""

import argparse
import sys
import os
import json
import csv
import time
import logging
from pathlib import Path
from datetime import datetime
from typing import Optional, List, Dict, Any, Callable, Tuple
from dataclasses import dataclass, asdict
from enum import Enum
import threading
from contextlib import contextmanager

import cv2
import numpy as np

# 确保src目录在路径中
sys.path.insert(0, str(Path(__file__).parent))

from src.vision import (
    GameStateDetector,
    DetectorConfig,
    GameState,
    DetectionResult,
    detect_game_state,
    EASYOCR_AVAILABLE,
)
from src.vision.gui_matcher import (
    GUIMatcher,
    GUIMatcherConfig,
    MatchResult,
    MatchMethod,
)
from src.vision.enhanced_gui_matcher import (
    MainMenuAnalyzer,
    UIElement,
    UIElementType,
)
from src.data import (
    DataManager,
    ManagerConfig,
    CacheConfig,
    Operator,
    Stage,
    Item,
)
from src.vision.squad_recognizer import SquadRecognizer, SquadConfig
from src.vision.squad_analyzer import SquadAnalyzer


# =============================================================================
# 常量定义
# =============================================================================

APP_NAME = "Arknights Game State Detector"
APP_VERSION = "1.0.0"
DEFAULT_CONFIG_PATH = Path.home() / ".arknights_detector" / "config.json"
DEFAULT_LOG_PATH = Path.home() / ".arknights_detector" / "logs"


class OutputFormat(Enum):
    """输出格式枚举"""
    JSON = "json"
    CSV = "csv"
    TXT = "txt"
    CONSOLE = "console"


# =============================================================================
# 日志配置
# =============================================================================

def setup_logging(
    level: str = "INFO",
    log_file: Optional[Path] = None,
    console: bool = True
) -> logging.Logger:
    """
    配置日志系统

    Args:
        level: 日志级别 (DEBUG/INFO/WARNING/ERROR)
        log_file: 日志文件路径
        console: 是否输出到控制台

    Returns:
        配置好的Logger实例
    """
    logger = logging.getLogger("arknights_detector")
    logger.setLevel(getattr(logging, level.upper()))
    logger.handlers = []  # 清除已有处理器

    formatter = logging.Formatter(
        '[%(asctime)s] [%(levelname)s] %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S'
    )

    # 文件处理器
    if log_file:
        log_file.parent.mkdir(parents=True, exist_ok=True)
        file_handler = logging.FileHandler(log_file, encoding='utf-8')
        file_handler.setFormatter(formatter)
        logger.addHandler(file_handler)

    # 控制台处理器
    if console:
        console_handler = logging.StreamHandler(sys.stdout)
        console_handler.setFormatter(formatter)
        logger.addHandler(console_handler)

    return logger


# =============================================================================
# 配置管理
# =============================================================================

@dataclass
class CLIConfig:
    """CLI配置类"""
    detector: Dict[str, Any] = None
    output_format: str = "console"
    auto_save: bool = False
    save_dir: str = str(Path.home() / ".arknights_detector" / "results")
    log_level: str = "INFO"
    enable_gpu: bool = True

    def __post_init__(self):
        if self.detector is None:
            self.detector = {}

    @classmethod
    def load(cls, path: Path = DEFAULT_CONFIG_PATH) -> 'CLIConfig':
        """从文件加载配置"""
        if path.exists():
            with open(path, 'r', encoding='utf-8') as f:
                data = json.load(f)
            return cls(**data)
        return cls()

    def save(self, path: Path = DEFAULT_CONFIG_PATH) -> None:
        """保存配置到文件"""
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, 'w', encoding='utf-8') as f:
            json.dump(asdict(self), f, indent=2, ensure_ascii=False)


# =============================================================================
# 结果格式化
# =============================================================================

class ResultFormatter:
    """结果格式化器"""

    @staticmethod
    def format_console(result: DetectionResult, filename: str = "") -> str:
        """格式化为控制台输出"""
        lines = [
            "=" * 60,
            f"文件: {filename}" if filename else "检测结果",
            "=" * 60,
            f"  游戏状态: {result.state.name}",
            f"  置信度: {result.confidence:.3f}",
            f"  识别文本: '{result.raw_text}'",
            f"  匹配关键词: {', '.join(result.matched_keywords) if result.matched_keywords else '无'}",
            f"  处理时间: {result.processing_time_ms:.2f}ms",
        ]
        if result.error_message:
            lines.append(f"  错误: {result.error_message}")
        lines.append("=" * 60)
        return "\n".join(lines)

    @staticmethod
    def format_json(results: List[Dict[str, Any]]) -> str:
        """格式化为JSON"""
        return json.dumps(results, indent=2, ensure_ascii=False)

    @staticmethod
    def format_csv(results: List[Dict[str, Any]]) -> str:
        """格式化为CSV"""
        if not results:
            return ""

        output = []
        writer = csv.DictWriter(output, fieldnames=results[0].keys())
        writer.writeheader()
        writer.writerows(results)
        return "\n".join(output)


# =============================================================================
# 图像处理工具
# =============================================================================

class ImageLoader:
    """图像加载器"""

    SUPPORTED_EXTENSIONS = {'.png', '.jpg', '.jpeg', '.bmp', '.tiff', '.webp'}

    @classmethod
    def load(cls, path: Path) -> Optional[np.ndarray]:
        """加载单张图像"""
        if not path.exists():
            return None

        if path.suffix.lower() not in cls.SUPPORTED_EXTENSIONS:
            return None

        image = cv2.imread(str(path))
        return image

    @classmethod
    def load_batch(cls, directory: Path, recursive: bool = False) -> List[Tuple[Path, np.ndarray]]:
        """批量加载图像"""
        results = []

        pattern = "**/*" if recursive else "*"
        for file_path in directory.glob(pattern):
            if file_path.suffix.lower() in cls.SUPPORTED_EXTENSIONS:
                image = cls.load(file_path)
                if image is not None:
                    results.append((file_path, image))

        return results


# =============================================================================
# 核心命令实现
# =============================================================================

class DetectorCommands:
    """检测命令实现"""

    def __init__(self, config: CLIConfig, logger: logging.Logger):
        self.config = config
        self.logger = logger
        self._detector: Optional[GameStateDetector] = None

    def _get_detector(self) -> GameStateDetector:
        """获取或创建检测器"""
        if self._detector is None:
            detector_config = DetectorConfig(
                use_gpu=self.config.enable_gpu,
                **self.config.detector
            )
            self._detector = GameStateDetector(detector_config)
            self._detector.initialize()
        return self._detector

    def _release_detector(self):
        """释放检测器"""
        if self._detector:
            self._detector.shutdown()
            self._detector = None

    def detect_single(
        self,
        image_path: Path,
        visualize: bool = False,
        save_roi: bool = False
    ) -> DetectionResult:
        """检测单张图像"""
        self.logger.info(f"检测图像: {image_path}")

        image = ImageLoader.load(image_path)
        if image is None:
            raise ValueError(f"无法加载图像: {image_path}")

        detector = self._get_detector()
        # 单张检测禁用状态平滑，直接返回检测结果
        result = detector.detect(image, return_roi=save_roi, use_smoothing=False)

        # 打印结果
        print(ResultFormatter.format_console(result, str(image_path)))

        # 可视化
        if visualize:
            self._visualize_result(image, result)

        # 保存ROI
        if save_roi and result.roi_image is not None:
            roi_path = Path(self.config.save_dir) / f"{image_path.stem}_roi.png"
            roi_path.parent.mkdir(parents=True, exist_ok=True)
            cv2.imwrite(str(roi_path), result.roi_image)
            self.logger.info(f"ROI已保存: {roi_path}")

        return result

    def detect_batch(
        self,
        directory: Path,
        output: Optional[Path] = None,
        format: OutputFormat = OutputFormat.CONSOLE,
        recursive: bool = False
    ) -> List[Dict[str, Any]]:
        """批量检测"""
        self.logger.info(f"批量检测目录: {directory}")

        images = ImageLoader.load_batch(directory, recursive)
        if not images:
            self.logger.warning("未找到图像文件")
            return []

        self.logger.info(f"找到 {len(images)} 个图像文件")

        results = []
        detector = self._get_detector()

        for i, (path, image) in enumerate(images, 1):
            self.logger.info(f"[{i}/{len(images)}] 处理: {path.name}")

            try:
                # 批量检测禁用状态平滑
                result = detector.detect(image, use_smoothing=False)

                result_dict = {
                    'file': str(path),
                    'filename': path.name,
                    'state': result.state.name,
                    'confidence': result.confidence,
                    'raw_text': result.raw_text,
                    'matched_keywords': ', '.join(result.matched_keywords),
                    'processing_time_ms': result.processing_time_ms,
                    'timestamp': datetime.now().isoformat(),
                    'error': result.error_message or ''
                }
                results.append(result_dict)

                # 控制台输出
                if format == OutputFormat.CONSOLE:
                    print(ResultFormatter.format_console(result, path.name))

            except Exception as e:
                self.logger.error(f"处理 {path.name} 时出错: {e}")
                results.append({
                    'file': str(path),
                    'filename': path.name,
                    'state': 'ERROR',
                    'error': str(e),
                    'timestamp': datetime.now().isoformat()
                })

        # 保存结果
        if output:
            self._save_results(results, output, format)

        # 打印统计
        self._print_statistics(results)

        return results

    def monitor(
        self,
        interval: float = 1.0,
        duration: Optional[float] = None,
        on_state_change: Optional[Callable[[DetectionResult], None]] = None
    ):
        """实时监控屏幕"""
        try:
            import mss
        except ImportError:
            self.logger.error("需要安装 mss 库: pip install mss")
            return

        self.logger.info(f"启动实时监控 (间隔: {interval}s)")
        if duration:
            self.logger.info(f"监控时长: {duration}s")

        detector = self._get_detector()
        monitor_start = time.time()
        last_state = None

        try:
            with mss.mss() as sct:
                monitor = sct.monitors[1]

                while True:
                    # 检查时长
                    if duration and (time.time() - monitor_start) >= duration:
                        self.logger.info("监控时长已到，停止监控")
                        break

                    # 捕获屏幕
                    screenshot = np.array(sct.grab(monitor))
                    image = cv2.cvtColor(screenshot, cv2.COLOR_RGBA2BGR)

                    # 检测
                    result = detector.detect(image)

                    # 状态变化回调
                    if result.state != last_state:
                        self.logger.info(
                            f"状态变化: {last_state} -> {result.state.name} "
                            f"(置信度: {result.confidence:.3f})"
                        )
                        if on_state_change:
                            on_state_change(result)
                        last_state = result.state

                    # 显示实时信息
                    self._print_monitor_status(result)

                    # 等待
                    time.sleep(interval)

        except KeyboardInterrupt:
            self.logger.info("用户中断监控")

    def gui_match(
        self,
        image_path: Path,
        template: Optional[str] = None,
        texts: Optional[List[str]] = None,
        main_menu: bool = False,
        output: Optional[Path] = None,
        threshold: float = 0.8
    ) -> bool:
        """GUI界面元素匹配"""
        self.logger.info(f"GUI匹配: {image_path}")

        # 加载图像
        image = cv2.imread(str(image_path))
        if image is None:
            self.logger.error(f"无法加载图像: {image_path}")
            return False

        # 初始化GUI匹配器
        config = GUIMatcherConfig(
            use_gpu=self.config.enable_gpu,
            default_template_threshold=threshold
        )
        matcher = GUIMatcher(config)

        if not matcher.initialize():
            self.logger.error("GUI匹配器初始化失败")
            return False

        try:
            results = []

            if template:
                # 模板匹配模式
                self.logger.info(f"模板匹配: {template}")
                result = matcher.match_template(image, template, threshold)
                if result:
                    results.append(result)
                    print(f"✓ 找到模板 '{template}':")
                    print(f"  位置: {result.position}")
                    print(f"  中心: {result.center}")
                    print(f"  置信度: {result.confidence:.3f}")
                else:
                    print(f"✗ 未找到模板 '{template}'")

            elif texts:
                # OCR文字匹配模式
                self.logger.info(f"OCR匹配: {texts}")
                results = matcher.match_text(image, texts)
                if results:
                    print(f"✓ 找到 {len(results)} 个匹配:")
                    for r in results:
                        print(f"  '{r.matched_text}' -> '{r.name}'")
                        print(f"    位置: {r.position}, 中心: {r.center}")
                        print(f"    置信度: {r.confidence:.3f}")
                else:
                    print(f"✗ 未找到文字: {texts}")

            elif main_menu:
                # 主界面按钮查找
                self.logger.info("查找主界面按钮")
                menu_results = matcher.find_main_menu_buttons(image)

                print("\n主界面按钮检测结果:")
                print("-" * 50)

                found_count = 0
                for name, result in menu_results.items():
                    if result:
                        found_count += 1
                        results.append(result)
                        print(f"✓ {name:12s}: 位置={result.center}, 置信度={result.confidence:.3f}")
                        if result.matched_text:
                            print(f"              识别文字: '{result.matched_text}'")
                    else:
                        print(f"✗ {name:12s}: 未找到")

                print("-" * 50)
                print(f"找到 {found_count}/{len(menu_results)} 个按钮")

            else:
                print("请指定匹配模式: --template, --text 或 --main-menu")
                return False

            # 可视化
            if output and results:
                vis = matcher.visualize_matches(image, results, output)
                self.logger.info(f"可视化结果已保存: {output}")

                # 显示结果
                cv2.imshow("GUI Match Result", vis)
                print("\n按任意键关闭可视化窗口...")
                cv2.waitKey(0)
                cv2.destroyAllWindows()

            return len(results) > 0

        finally:
            matcher.shutdown()

    def analyze_main_menu(
        self,
        image_path: Path,
        output: Optional[Path] = None,
        show_vis: bool = True,
        debug: bool = False,
        debug_dir: Optional[Path] = None
    ) -> bool:
        """分析主界面"""
        self.logger.info(f"分析主界面: {image_path}")

        # 加载图像
        image = cv2.imread(str(image_path))
        if image is None:
            self.logger.error(f"无法加载图像: {image_path}")
            return False

        # 初始化分析器
        analyzer = MainMenuAnalyzer(use_gpu=self.config.enable_gpu)
        if not analyzer.initialize():
            self.logger.error("分析器初始化失败")
            return False

        try:
            # 分析主界面（带调试选项）
            result = analyzer.analyze(
                image,
                save_debug_images=debug,
                debug_dir=debug_dir
            )

            # 打印结果
            print("\n" + "=" * 60)
            print("主界面分析结果")
            print("=" * 60)

            # 按钮
            print("\n【按钮识别】")
            print("-" * 40)
            found_count = 0
            for name, elem in result.buttons.items():
                if elem:
                    found_count += 1
                    print(f"✓ {name:12s}: 位置={elem.center}, 置信度={elem.confidence:.3f}")
                    if elem.text:
                        print(f"              识别文字: '{elem.text}'")
                else:
                    print(f"✗ {name:12s}: 未找到")
            print(f"\n找到 {found_count}/{len(result.buttons)} 个按钮")

            # 终端
            if result.terminal:
                print("\n【终端】")
                print(f"  名称: {result.terminal.name}")
                print(f"  位置: {result.terminal.position}")

            # 理智
            if result.sanity:
                print("\n【理智】")
                print(f"  当前/上限: {result.sanity}")
                print(f"  位置: {result.sanity.position}")

            # 活动
            if result.activities:
                print("\n【活动】")
                for act in result.activities[:5]:
                    print(f"  - {act.name} @ {act.position}")

            # 资源
            if result.resources:
                print("\n【资源】")
                for res_id, res in result.resources.items():
                    print(f"  {res.name}: {res.amount:,} @ {res.position}")

            print("=" * 60)

            # 可视化
            if output or show_vis:
                vis_path = output or Path("main_menu_analysis.jpg")
                vis = analyzer.visualize(image, result, vis_path)
                self.logger.info(f"可视化结果已保存: {vis_path}")

                if show_vis:
                    cv2.imshow("Main Menu Analysis", vis)
                    print("\n按任意键关闭可视化窗口...")
                    cv2.waitKey(0)
                    cv2.destroyAllWindows()

            return found_count > 0

        finally:
            analyzer.shutdown()

    def test(self) -> bool:
        """运行自检"""
        self.logger.info("运行自检...")
        print("=" * 60)
        print("自检报告")
        print("=" * 60)

        checks = []

        # 检查1: OpenCV
        try:
            import cv2
            checks.append(("OpenCV", True, f"版本 {cv2.__version__}"))
        except ImportError:
            checks.append(("OpenCV", False, "未安装"))

        # 检查2: EasyOCR
        checks.append(("EasyOCR", EASYOCR_AVAILABLE,
                      "可用" if EASYOCR_AVAILABLE else "未安装"))

        # 检查3: PyTorch
        try:
            import torch
            cuda_available = torch.cuda.is_available()
            checks.append(("PyTorch", True,
                          f"版本 {torch.__version__}, CUDA: {cuda_available}"))
        except ImportError:
            checks.append(("PyTorch", False, "未安装"))

        # 检查4: NumPy
        try:
            import numpy as np
            checks.append(("NumPy", True, f"版本 {np.__version__}"))
        except ImportError:
            checks.append(("NumPy", False, "未安装"))

        # 检查5: 检测器初始化
        try:
            detector = self._get_detector()
            checks.append(("检测器初始化", True, "成功"))
        except Exception as e:
            checks.append(("检测器初始化", False, str(e)))

        # 打印结果
        all_passed = True
        for name, passed, info in checks:
            status = "✓" if passed else "✗"
            print(f"  [{status}] {name}: {info}")
            if not passed:
                all_passed = False

        print("=" * 60)

        if all_passed:
            self.logger.info("所有检查通过")
        else:
            self.logger.error("部分检查失败")

        return all_passed

    def _visualize_result(self, image: np.ndarray, result: DetectionResult):
        """可视化结果"""
        vis = image.copy()
        h, w = vis.shape[:2]

        # 绘制状态
        if result.state == GameState.IN_BATTLE:
            color = (0, 255, 0)
            text = "IN BATTLE"
        elif result.state == GameState.NOT_IN_BATTLE:
            color = (0, 0, 255)
            text = "NOT IN BATTLE"
        else:
            color = (0, 255, 255)
            text = "UNKNOWN"

        # 绘制信息栏
        cv2.rectangle(vis, (0, 0), (w, 80), (0, 0, 0), -1)
        cv2.putText(vis, f"State: {text}", (10, 35),
                   cv2.FONT_HERSHEY_SIMPLEX, 1.0, color, 2)
        cv2.putText(vis, f"Confidence: {result.confidence:.3f}", (10, 65),
                   cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 1)

        # 绘制ROI区域
        config = DetectorConfig()
        roi_x, roi_y, roi_w, roi_h = config.get_absolute_roi(w, h)
        cv2.rectangle(vis, (roi_x, roi_y), (roi_x + roi_w, roi_y + roi_h),
                     color, 3)

        cv2.imshow("Detection Result", vis)
        print("按任意键关闭窗口...")
        cv2.waitKey(0)
        cv2.destroyAllWindows()

    def _save_results(
        self,
        results: List[Dict[str, Any]],
        output: Path,
        format: OutputFormat
    ):
        """保存结果到文件"""
        output.parent.mkdir(parents=True, exist_ok=True)

        if format == OutputFormat.JSON:
            with open(output, 'w', encoding='utf-8') as f:
                json.dump(results, f, indent=2, ensure_ascii=False)

        elif format == OutputFormat.CSV:
            with open(output, 'w', newline='', encoding='utf-8') as f:
                if results:
                    writer = csv.DictWriter(f, fieldnames=results[0].keys())
                    writer.writeheader()
                    writer.writerows(results)

        elif format == OutputFormat.TXT:
            with open(output, 'w', encoding='utf-8') as f:
                for r in results:
                    f.write(f"{r['filename']}: {r['state']} (conf={r.get('confidence', 0):.3f})\n")

        self.logger.info(f"结果已保存: {output}")

    def _print_statistics(self, results: List[Dict[str, Any]]):
        """打印统计信息"""
        if not results:
            return

        states = {}
        for r in results:
            state = r.get('state', 'UNKNOWN')
            states[state] = states.get(state, 0) + 1

        print("\n" + "=" * 60)
        print("统计信息")
        print("=" * 60)
        print(f"  总图像数: {len(results)}")
        for state, count in sorted(states.items()):
            percentage = count / len(results) * 100
            print(f"  {state}: {count} ({percentage:.1f}%)")
        print("=" * 60)

    def _print_monitor_status(self, result: DetectionResult):
        """打印监控状态"""
        status = f"\r[{datetime.now().strftime('%H:%M:%S')}] "
        status += f"State: {result.state.name:15} "
        status += f"Conf: {result.confidence:.3f} "
        status += f"Text: '{result.raw_text[:20]:20}'"
        print(status, end='', flush=True)


# =============================================================================
# 数据管理命令
# =============================================================================

class DataCommands:
    """数据管理命令实现"""

    def __init__(self, logger: logging.Logger):
        self.logger = logger
        self._manager: Optional[DataManager] = None

    def _get_manager(self) -> DataManager:
        """获取或创建数据管理器"""
        if self._manager is None:
            config = ManagerConfig()
            self._manager = DataManager(config)
            if not self._manager.initialize():
                raise RuntimeError("数据管理器初始化失败")
        return self._manager

    def _release_manager(self):
        """释放数据管理器"""
        if self._manager:
            self._manager.shutdown()
            self._manager = None

    def sync(self, force: bool = False) -> bool:
        """
        同步GitHub数据

        Args:
            force: 是否强制同步
        """
        self.logger.info("开始同步数据...")
        manager = self._get_manager()

        try:
            success = manager.sync_github(force=force)
            if success:
                print("✓ 数据同步成功")
                # 加载所有数据到本地数据库
                print("正在加载数据到本地数据库...")
                manager.load_all_data(
                    progress_callback=lambda t, c, total: print(
                        f"  加载{t}: {c}/{total}", end='\r'
                    )
                )
                print("\n✓ 数据加载完成")
            else:
                print("✗ 数据同步失败")
            return success

        finally:
            self._release_manager()

    def load(self) -> bool:
        """加载所有数据"""
        self.logger.info("加载数据...")
        manager = self._get_manager()

        try:
            print("\n开始加载数据...")
            print("-" * 50)

            def progress_callback(data_type: str, current: int, total: int):
                """进度回调"""
                type_names = {
                    'operator': '干员',
                    'stage': '关卡',
                    'item': '物品',
                    'enemy': '敌人',
                    'complete': '完成'
                }
                name = type_names.get(data_type, data_type)

                if data_type == 'complete':
                    print(f"\r✓ {name}: {current}/{total}", end='')
                else:
                    percent = (current / total * 100) if total > 0 else 0
                    print(f"\r  加载{name}: {current}/{total} ({percent:.1f}%)", end='')

            success = manager.load_all_data(progress_callback=progress_callback)
            print()  # 换行
            print("-" * 50)

            if success:
                stats = manager.get_stats()
                print(f"✓ 数据加载完成!")
                print(f"  - 干员: {stats['operators_count']} 个")
                print(f"  - 关卡: {stats['stages_count']} 个")
                print(f"  - 物品: {stats['items_count']} 个")
                print(f"  - 敌人: {stats['enemies_count']} 个")
            else:
                print("✗ 数据加载失败")

            return success

        finally:
            self._release_manager()

    def load_structured(self) -> bool:
        """加载所有数据到结构化数据库"""
        self.logger.info("加载数据到结构化数据库...")
        manager = self._get_manager()

        try:
            print("\n开始加载数据到结构化数据库...")
            print("=" * 60)

            def progress_callback(data_type: str, current: int, total: int):
                """进度回调"""
                type_names = {
                    'operator': '干员',
                    'stage': '关卡',
                    'item': '物品',
                    'enemy': '敌人',
                    'complete': '完成'
                }
                name = type_names.get(data_type, data_type)

                if data_type == 'complete':
                    print(f"\r✓ {name}: {current}/{total}", end='')
                else:
                    percent = (current / total * 100) if total > 0 else 0
                    print(f"\r  结构化加载{name}: {current}/{total} ({percent:.1f}%)", end='')

            success = manager.load_all_data_structured(progress_callback=progress_callback)
            print()  # 换行
            print("=" * 60)

            if success:
                stats = manager.get_structured_stats()
                print(f"✓ 结构化数据加载完成!")
                print(f"\n数据库统计:")
                print(f"  - 干员: {stats.get('operators_count', 0)} 个")
                print(f"  - 关卡: {stats.get('stages_count', 0)} 个")
                print(f"  - 物品: {stats.get('items_count', 0)} 个")
                print(f"  - 敌人: {stats.get('enemies_count', 0)} 个")

                # 显示分布统计
                if 'operator_professions' in stats:
                    print(f"\n干员职业分布:")
                    for prof in stats['operator_professions'][:5]:
                        print(f"  - {prof['profession']}: {prof['count']} 个")

                if 'operator_rarities' in stats:
                    print(f"\n干员稀有度分布:")
                    for rarity in stats['operator_rarities']:
                        print(f"  - {rarity['rarity']}星: {rarity['count']} 个")

                if 'stage_types' in stats:
                    print(f"\n关卡类型分布:")
                    for st in stats['stage_types'][:5]:
                        print(f"  - {st['stage_type']}: {st['count']} 个")

                if 'item_types' in stats:
                    print(f"\n物品类型分布:")
                    for it in stats['item_types'][:5]:
                        print(f"  - {it['item_type']}: {it['count']} 个")

                if 'enemy_levels' in stats:
                    print(f"\n敌人等级分布:")
                    for el in stats['enemy_levels']:
                        print(f"  - {el['enemy_level']}: {el['count']} 个")
            else:
                print("✗ 结构化数据加载失败")

            return success

        finally:
            self._release_manager()

    def query_structured(self, entity_type: str, **kwargs) -> bool:
        """
        使用结构化数据库查询

        Args:
            entity_type: 实体类型 (operator, stage, item, enemy)
            **kwargs: 查询参数
        """
        manager = self._get_manager()

        try:
            if entity_type == 'operator':
                return self._query_structured_operators(manager, **kwargs)
            elif entity_type == 'stage':
                return self._query_structured_stages(manager, **kwargs)
            elif entity_type == 'item':
                return self._query_structured_items(manager, **kwargs)
            elif entity_type == 'enemy':
                return self._query_structured_enemies(manager, **kwargs)
            else:
                print(f"未知实体类型: {entity_type}")
                return False

        finally:
            self._release_manager()

    def _query_structured_operators(self, manager: DataManager, **kwargs) -> bool:
        """查询结构化干员数据"""
        from src.data.models.operator import OperatorProfession, OperatorRarity, PositionType

        # 解析参数
        profession = kwargs.get('profession')
        if profession:
            try:
                profession = OperatorProfession(profession.upper())
            except ValueError:
                pass

        rarity = kwargs.get('rarity')
        if rarity:
            try:
                rarity = OperatorRarity(int(rarity))
            except ValueError:
                pass

        min_rarity = kwargs.get('min_rarity')
        max_rarity = kwargs.get('max_rarity')
        nation_id = kwargs.get('nation_id')
        team_id = kwargs.get('team_id')
        is_robot = kwargs.get('is_robot')
        if is_robot is not None:
            is_robot = is_robot.lower() in ('true', '1', 'yes')
        limit = int(kwargs.get('limit', 20))

        results = manager.query_operators_structured(
            profession=profession,
            rarity=rarity,
            min_rarity=int(min_rarity) if min_rarity else None,
            max_rarity=int(max_rarity) if max_rarity else None,
            nation_id=nation_id,
            team_id=int(team_id) if team_id else None,
            is_robot=is_robot,
            limit=limit
        )

        if results:
            print(f"\n找到 {len(results)} 个干员:")
            print("-" * 80)
            print(f"{'ID':<20} {'名称':<15} {'职业':<10} {'稀有度':<8} {'位置':<10}")
            print("-" * 80)
            for op in results:
                print(f"{op['id']:<20} {op['name']:<15} {op['profession']:<10} "
                      f"{op['rarity']}星{' '*5} {op.get('position', 'N/A'):<10}")
            print("-" * 80)
            return True
        else:
            print("未找到匹配干员")
            return False

    def _query_structured_stages(self, manager: DataManager, **kwargs) -> bool:
        """查询结构化关卡数据"""
        from src.data.models.stage import StageType, Difficulty

        stage_type = kwargs.get('stage_type')
        if stage_type:
            try:
                stage_type = StageType(stage_type.upper())
            except ValueError:
                pass

        zone_id = kwargs.get('zone_id')
        difficulty = kwargs.get('difficulty')
        if difficulty:
            try:
                difficulty = Difficulty(difficulty.upper())
            except ValueError:
                pass

        min_ap_cost = kwargs.get('min_ap_cost')
        max_ap_cost = kwargs.get('max_ap_cost')
        can_practice = kwargs.get('can_practice')
        if can_practice is not None:
            can_practice = can_practice.lower() in ('true', '1', 'yes')
        limit = int(kwargs.get('limit', 20))

        results = manager.query_stages_structured(
            stage_type=stage_type,
            zone_id=zone_id,
            difficulty=difficulty,
            min_ap_cost=int(min_ap_cost) if min_ap_cost else None,
            max_ap_cost=int(max_ap_cost) if max_ap_cost else None,
            can_practice=can_practice,
            limit=limit
        )

        if results:
            print(f"\n找到 {len(results)} 个关卡:")
            print("-" * 100)
            print(f"{'ID':<25} {'代码':<10} {'名称':<20} {'类型':<12} {'消耗':<6} {'经验':<6}")
            print("-" * 100)
            for stage in results:
                print(f"{stage['id']:<25} {stage.get('code', 'N/A'):<10} "
                      f"{stage['name']:<20} {stage['stage_type']:<12} "
                      f"{stage['ap_cost']:<6} {stage['exp_gain']:<6}")
            print("-" * 100)
            return True
        else:
            print("未找到匹配关卡")
            return False

    def _query_structured_items(self, manager: DataManager, **kwargs) -> bool:
        """查询结构化物品数据"""
        from src.data.models.item import ItemType

        item_type = kwargs.get('item_type')
        if item_type:
            try:
                item_type = ItemType(item_type.upper())
            except ValueError:
                pass

        rarity = kwargs.get('rarity')
        min_rarity = kwargs.get('min_rarity')
        max_rarity = kwargs.get('max_rarity')
        is_material = kwargs.get('is_material')
        if is_material is not None:
            is_material = is_material.lower() in ('true', '1', 'yes')
        is_exp_card = kwargs.get('is_exp_card')
        if is_exp_card is not None:
            is_exp_card = is_exp_card.lower() in ('true', '1', 'yes')
        limit = int(kwargs.get('limit', 20))

        results = manager.query_items_structured(
            item_type=item_type,
            rarity=int(rarity) if rarity else None,
            min_rarity=int(min_rarity) if min_rarity else None,
            max_rarity=int(max_rarity) if max_rarity else None,
            is_material=is_material,
            is_exp_card=is_exp_card,
            limit=limit
        )

        if results:
            print(f"\n找到 {len(results)} 个物品:")
            print("-" * 80)
            print(f"{'ID':<25} {'名称':<20} {'类型':<20} {'稀有度':<8}")
            print("-" * 80)
            for item in results:
                print(f"{item['id']:<25} {item['name']:<20} "
                      f"{item['item_type']:<20} {item['rarity']}星")
            print("-" * 80)
            return True
        else:
            print("未找到匹配物品")
            return False

    def _query_structured_enemies(self, manager: DataManager, **kwargs) -> bool:
        """查询结构化敌人数据"""
        from src.data.models.enemy import EnemyLevel

        enemy_level = kwargs.get('enemy_level')
        if enemy_level:
            try:
                enemy_level = EnemyLevel(enemy_level.upper())
            except ValueError:
                pass

        min_hp = kwargs.get('min_hp')
        max_hp = kwargs.get('max_hp')
        min_atk = kwargs.get('min_atk')
        max_atk = kwargs.get('max_atk')
        limit = int(kwargs.get('limit', 20))

        results = manager.query_enemies_structured(
            enemy_level=enemy_level,
            min_hp=int(min_hp) if min_hp else None,
            max_hp=int(max_hp) if max_hp else None,
            min_atk=int(min_atk) if min_atk else None,
            max_atk=int(max_atk) if max_atk else None,
            limit=limit
        )

        if results:
            print(f"\n找到 {len(results)} 个敌人:")
            print("-" * 100)
            print(f"{'ID':<25} {'名称':<20} {'等级':<10} {'生命':<10} {'攻击':<8} {'防御':<8}")
            print("-" * 100)
            for enemy in results:
                print(f"{enemy['id']:<25} {enemy['name']:<20} "
                      f"{enemy['enemy_level']:<10} {enemy['max_hp']:<10} "
                      f"{enemy['atk']:<8} {enemy['def']:<8}")
            print("-" * 100)
            return True
        else:
            print("未找到匹配敌人")
            return False

    def get_material_tree(self, item_id: str) -> bool:
        """获取材料合成树"""
        manager = self._get_manager()

        try:
            tree = manager.get_material_tree(item_id)
            if tree and 'error' not in tree:
                self._print_material_tree(tree)
                return True
            else:
                print(f"未找到材料: {item_id}")
                return False

        finally:
            self._release_manager()

    def _print_material_tree(self, tree: Dict[str, Any], indent: int = 0):
        """打印材料树"""
        prefix = "  " * indent
        if 'name' in tree:
            count = tree.get('count', 1)
            count_str = f" x{count}" if count > 1 else ""
            print(f"{prefix}├─ {tree['name']}{count_str} ({tree.get('rarity', '?')}★)")
        else:
            print(f"{prefix}├─ {tree['item_id']}")

        if 'cost_gold' in tree:
            print(f"{prefix}│  消耗龙门币: {tree['cost_gold']}")

        for material in tree.get('materials', []):
            self._print_material_tree(material, indent + 1)

    def stats(self) -> bool:
        """显示数据统计"""
        manager = self._get_manager()

        try:
            stats = manager.get_stats()

            print("\n" + "=" * 60)
            print("数据统计")
            print("=" * 60)
            print(f"初始化状态: {'✓' if stats['initialized'] else '✗'}")
            print(f"干员数量: {stats['operators_count']}")
            print(f"关卡数量: {stats['stages_count']}")
            print(f"物品数量: {stats['items_count']}")
            print(f"敌人数量: {stats['enemies_count']}")
            print(f"内存缓存条目: {stats['memory_cache_entries']}")
            print("=" * 60)

            # GitHub统计
            if 'github_stats' in stats:
                gh = stats['github_stats']
                print("\nGitHub数据:")
                print(f"  初始化: {'✓' if gh.get('initialized') else '✗'}")
                print(f"  数据版本: {gh.get('version', {}).get('version', 'unknown')}")

            # PRTS统计
            if 'prts_stats' in stats:
                prts = stats['prts_stats']
                print("\nPRTS Wiki:")
                print(f"  初始化: {'✓' if prts.get('initialized') else '✗'}")
                print(f"  API端点: {prts.get('base_url', 'N/A')}")

            # 结构化数据库统计
            if 'structured_db' in stats:
                sdb = stats['structured_db']
                print("\n结构化数据库:")
                print(f"  干员: {sdb.get('operators_count', 0)} 个")
                print(f"  关卡: {sdb.get('stages_count', 0)} 个")
                print(f"  物品: {sdb.get('items_count', 0)} 个")
                print(f"  敌人: {sdb.get('enemies_count', 0)} 个")

            print("=" * 60)
            return True

        finally:
            self._release_manager()

    def query_operator(self, operator_id: Optional[str] = None, name: Optional[str] = None) -> bool:
        """
        查询干员信息

        Args:
            operator_id: 干员ID
            name: 干员名称
        """
        manager = self._get_manager()

        try:
            if operator_id:
                operator = manager.get_operator(operator_id)
                if operator:
                    self._print_operator(operator)
                    return True
                else:
                    print(f"未找到干员: {operator_id}")
                    return False

            elif name:
                # 按名称搜索
                operators = manager.get_operators(
                    filter_func=lambda op: name.lower() in op.name.lower()
                )
                if operators:
                    print(f"\n找到 {len(operators)} 个干员:")
                    for op in operators[:10]:  # 最多显示10个
                        self._print_operator_summary(op)
                    return True
                else:
                    print(f"未找到匹配 '{name}' 的干员")
                    return False

            else:
                # 列出所有干员
                operators = manager.get_operators(
                    sort_key=lambda op: (-op.rarity.value, op.name)
                )
                print(f"\n共有 {len(operators)} 个干员")
                print("\n前20个6星干员:")
                for op in operators[:20]:
                    if op.rarity.value == 6:
                        self._print_operator_summary(op)
                return True

        finally:
            self._release_manager()

    def _print_operator(self, op: Operator):
        """打印干员详细信息"""
        # 位置显示映射
        position_map = {
            'MELEE': '近战位',
            'RANGED': '远程位',
            'ALL': '均可',
            'NONE': '无'
        }

        print("\n" + "=" * 70)
        print(f"【{op.name}】")
        print("=" * 70)

        # 基础信息
        print("\n【基础信息】")
        print(f"  干员ID: {op.id}")
        print(f"  代号: {op.appellation}")
        print(f"  职业: {op.profession.value}")
        print(f"  子职业: {op.sub_profession_id}")
        print(f"  星级: {'★' * op.stars}")
        print(f"  位置: {position_map.get(op.position.value, op.position.value)}")
        print(f"  编号: {op.display_number}")
        print(f"  势力: {op.nation_id or '无'}")
        if op.group_id:
            print(f"  组织: {op.group_id}")
        if op.team_id:
            print(f"  小队: {op.team_id}")

        # 描述
        print("\n【描述】")
        desc = op.description or '无'
        if len(desc) > 200:
            print(f"  {desc[:200]}...")
        else:
            print(f"  {desc}")

        # 获取方式
        if op.obtain_approach:
            print(f"\n【获取方式】")
            print(f"  {op.obtain_approach}")

        # 标签
        if op.tag_list:
            print(f"\n【标签】")
            print(f"  {', '.join(op.tag_list)}")

        # 属性数据
        if op.phases:
            print(f"\n【属性数据】")
            print(f"  精英化阶段数: {len(op.phases)}")
            for phase in op.phases:
                phase_name = {0: '精0', 1: '精一', 2: '精二'}.get(phase.phase_index, f"精{phase.phase_index}")
                print(f"\n  {phase_name} (等级上限: {phase.max_level}):")
                print(f"    生命值: {phase.max_hp}")
                print(f"    攻击力: {phase.atk}")
                print(f"    防御力: {phase.def_}")
                print(f"    法术抗性: {phase.magic_resistance}")
                print(f"    部署费用: {phase.cost}")
                print(f"    阻挡数: {phase.block_count}")
                print(f"    攻击速度: {phase.attack_speed}")
                print(f"    再部署时间: {phase.respawn_time}秒")

        # 技能
        if op.skills:
            print(f"\n【技能】 (共{len(op.skills)}个)")
            for i, skill in enumerate(op.skills, 1):
                print(f"\n  技能{i}: {skill.skill_name}")
                print(f"    ID: {skill.skill_id}")
                print(f"    SP消耗: {skill.sp_cost}")
                print(f"    初始SP: {skill.sp_initial}")
                print(f"    持续时间: {skill.duration}秒")
                desc = skill.description or '无'
                if len(desc) > 150:
                    print(f"    描述: {desc[:150]}...")
                else:
                    print(f"    描述: {desc}")

        # 天赋
        if op.talents:
            print(f"\n【天赋】 (共{len(op.talents)}个)")
            for i, talent in enumerate(op.talents, 1):
                unlock_phase_name = {0: '精0', 1: '精一', 2: '精二'}.get(talent.unlock_phase, f"精{talent.unlock_phase}")
                print(f"\n  天赋{i}: {talent.talent_name}")
                print(f"    ID: {talent.talent_id}")
                print(f"    解锁条件: {unlock_phase_name} {talent.unlock_level}级")
                desc = talent.description or '无'
                if len(desc) > 150:
                    print(f"    描述: {desc[:150]}...")
                else:
                    print(f"    描述: {desc}")

        # 潜能
        print(f"\n【潜能】")
        print(f"  最大潜能等级: {op.max_potential_level}")
        if op.potential_item_id:
            print(f"  潜能信物ID: {op.potential_item_id}")

        # 其他信息
        print(f"\n【其他信息】")
        print(f"  是否可获取: {'否' if op.is_not_obtainable else '是'}")
        print(f"  是否为异格: {'是' if op.is_sp_char else '否'}")

        print("\n" + "=" * 70)

    def _print_operator_summary(self, op: Operator):
        """打印干员摘要"""
        print(f"  [{op.stars}★] {op.name:12s} ({op.profession.value:8s}) - {op.id}")

    def query_stage(self, stage_id: Optional[str] = None, code: Optional[str] = None) -> bool:
        """
        查询关卡信息

        Args:
            stage_id: 关卡ID
            code: 关卡代码（如 1-7）
        """
        manager = self._get_manager()

        try:
            if stage_id:
                stage = manager.get_stage(stage_id)
                if stage:
                    self._print_stage(stage)
                    return True
                else:
                    print(f"未找到关卡: {stage_id}")
                    return False

            elif code:
                # 按代码搜索
                stages = manager.get_stages(
                    filter_func=lambda s: s.code == code
                )
                if stages:
                    for stage in stages:
                        self._print_stage(stage)
                    return True
                else:
                    print(f"未找到关卡代码: {code}")
                    return False

            else:
                # 列出主线关卡
                stages = manager.get_stages(
                    filter_func=lambda s: s.is_main_stage,
                    sort_key=lambda s: s.code
                )
                print(f"\n共有 {len(stages)} 个主线关卡")
                print("\n前20个主线关卡:")
                for stage in stages[:20]:
                    self._print_stage_summary(stage)
                return True

        finally:
            self._release_manager()

    def _print_stage(self, stage: Stage):
        """打印关卡详细信息"""
        print("\n" + "=" * 60)
        print(f"关卡: {stage.name} ({stage.code})")
        print("=" * 60)
        print(f"  ID: {stage.id}")
        print(f"  类型: {stage.stage_type.value}")
        print(f"  难度: {stage.difficulty.value}")
        print(f"  描述: {stage.description}")
        print(f"  理智消耗: {stage.ap_cost}")
        print(f"  经验获得: {stage.exp_gain}")
        print(f"  龙门币获得: {stage.gold_gain}")
        print(f"  推荐等级: {stage.danger_level}")
        print(f"  可演习: {'是' if stage.can_practice else '否'}")
        print(f"  可代理: {'是' if stage.can_battle_replay else '否'}")

        if stage.drops:
            print(f"\n  掉落物品:")
            for drop in stage.drops[:10]:
                print(f"    - {drop.item_name} ({drop.drop_type})")

        print("=" * 60)

    def _print_stage_summary(self, stage: Stage):
        """打印关卡摘要"""
        print(f"  {stage.code:8s} {stage.name:20s} 消耗{stage.ap_cost:2d}理智")

    def query_item(self, item_id: Optional[str] = None, name: Optional[str] = None) -> bool:
        """
        查询物品信息

        Args:
            item_id: 物品ID
            name: 物品名称
        """
        manager = self._get_manager()

        try:
            if item_id:
                item = manager.get_item(item_id)
                if item:
                    self._print_item(item)
                    return True
                else:
                    print(f"未找到物品: {item_id}")
                    return False

            elif name:
                # 按名称搜索
                items = manager.get_items(
                    filter_func=lambda i: name.lower() in i.name.lower()
                )
                if items:
                    print(f"\n找到 {len(items)} 个物品:")
                    for item in items[:10]:
                        self._print_item_summary(item)
                    return True
                else:
                    print(f"未找到匹配 '{name}' 的物品")
                    return False

            else:
                # 列出材料
                items = manager.get_items(
                    filter_func=lambda i: i.is_material,
                    sort_key=lambda i: (-i.rarity.value, i.name)
                )
                print(f"\n共有 {len(items)} 个材料")
                print("\n前20个高稀有度材料:")
                for item in items[:20]:
                    self._print_item_summary(item)
                return True

        finally:
            self._release_manager()

    def _print_item(self, item: Item):
        """打印物品详细信息"""
        print("\n" + "=" * 60)
        print(f"物品: {item.name}")
        print("=" * 60)
        print(f"  ID: {item.id}")
        print(f"  类型: {item.item_type.value}")
        print(f"  稀有度: {'★' * item.stars}")
        print(f"  描述: {item.description}")
        print(f"  用途: {item.usage}")
        print(f"  获取方式: {item.obtain_approach}")
        print("=" * 60)

    def _print_item_summary(self, item: Item):
        """打印物品摘要"""
        print(f"  [{item.stars}★] {item.name:20s} ({item.item_type.value})")

    def search_prts(self, query: str, limit: int = 10) -> bool:
        """
        在PRTS Wiki搜索

        Args:
            query: 搜索关键词
            limit: 返回结果数量
        """
        manager = self._get_manager()

        try:
            print(f"\n在PRTS Wiki搜索 '{query}'...")
            results = manager.search_prts(query, limit)

            if results:
                print(f"\n找到 {len(results)} 个结果:")
                for i, result in enumerate(results, 1):
                    print(f"\n{i}. {result['title']}")
                    print(f"   {result['snippet'][:100]}...")
                return True
            else:
                print("未找到结果")
                return False

        finally:
            self._release_manager()

    def get_prts_page(self, title: str) -> bool:
        """
        获取PRTS Wiki页面内容

        Args:
            title: 页面标题
        """
        manager = self._get_manager()

        try:
            print(f"\n获取页面 '{title}'...")
            content = manager.get_prts_page(title)

            if content:
                print("\n" + "=" * 60)
                print(f"页面内容: {title}")
                print("=" * 60)
                # 显示前2000字符
                print(content[:2000])
                if len(content) > 2000:
                    print(f"\n... (共 {len(content)} 字符)")
                print("=" * 60)
                return True
            else:
                print("未找到页面")
                return False

        finally:
            self._release_manager()


# =============================================================================
# 命令行参数解析
# =============================================================================

def create_parser() -> argparse.ArgumentParser:
    """创建参数解析器"""
    parser = argparse.ArgumentParser(
        prog='arknights-detector',
        description=f'{APP_NAME} v{APP_VERSION}',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 检测单张图像
  python cli.py detect -i screenshot.png

  # 批量检测并保存为JSON
  python cli.py detect -b ./screenshots/ -o results.json -f json

  # 实时监控屏幕60秒
  python cli.py monitor -d 60 -n 0.5

  # 运行自检
  python cli.py test

  # 显示配置
  python cli.py config --show

  # GUI模板匹配
  python cli.py gui -i screenshot.png -t home_btn.png

  # GUI OCR文字识别（主界面按钮）
  python cli.py gui -i screenshot.png --main-menu

  # GUI OCR查找特定文字
  python cli.py gui -i screenshot.png --text "编队" "干员" "任务"

  # 增强版主界面分析
  python cli.py main-menu -i screenshot.png -o result.jpg

  # 编队识别
  python cli.py squad -i squad.png -o result.jpg              # 编队选择界面（默认）
  python cli.py squad -i squad.png --layout edit -o result.jpg # 编队编辑界面
  python cli.py squad -i squad.png -j report.json             # 保存JSON报告

  # 数据管理
  python cli.py data sync                    # 同步GitHub数据
  python cli.py data sync --force           # 强制同步
  python cli.py data stats                  # 显示数据统计
  python cli.py data operator               # 列出所有干员
  python cli.py data operator -n "阿米娅"   # 搜索干员
  python cli.py data operator -i char_002_amiya  # 按ID查询
  python cli.py data stage                  # 列出所有关卡
  python cli.py data stage -c "1-7"        # 按代码查询
  python cli.py data item                   # 列出所有物品
  python cli.py data item -n "源石"        # 搜索物品
  python cli.py data search "银灰"          # PRTS Wiki搜索
  python cli.py data page "银灰"            # 获取PRTS页面
        """
    )

    parser.add_argument(
        '-v', '--version',
        action='version',
        version=f'%(prog)s {APP_VERSION}'
    )

    parser.add_argument(
        '--log-level',
        choices=['DEBUG', 'INFO', 'WARNING', 'ERROR'],
        default='INFO',
        help='日志级别 (默认: INFO)'
    )

    parser.add_argument(
        '--no-gpu',
        action='store_true',
        help='禁用GPU，使用CPU'
    )

    parser.add_argument(
        '--config',
        type=Path,
        default=DEFAULT_CONFIG_PATH,
        help=f'配置文件路径 (默认: {DEFAULT_CONFIG_PATH})'
    )

    # 子命令
    subparsers = parser.add_subparsers(dest='command', help='可用命令')

    # detect 命令
    detect_parser = subparsers.add_parser(
        'detect',
        help='检测游戏状态'
    )
    detect_parser.add_argument(
        '-i', '--image',
        type=Path,
        help='单张图像文件路径'
    )
    detect_parser.add_argument(
        '-b', '--batch',
        type=Path,
        help='批量检测目录'
    )
    detect_parser.add_argument(
        '-o', '--output',
        type=Path,
        help='输出文件路径'
    )
    detect_parser.add_argument(
        '-f', '--format',
        choices=['json', 'csv', 'txt', 'console'],
        default='console',
        help='输出格式 (默认: console)'
    )
    detect_parser.add_argument(
        '-r', '--recursive',
        action='store_true',
        help='递归处理子目录'
    )
    detect_parser.add_argument(
        '--visualize', '-v',
        action='store_true',
        help='显示可视化结果'
    )
    detect_parser.add_argument(
        '--save-roi',
        action='store_true',
        help='保存ROI区域图像'
    )

    # monitor 命令
    monitor_parser = subparsers.add_parser(
        'monitor',
        help='实时监控屏幕'
    )
    monitor_parser.add_argument(
        '-n', '--interval',
        type=float,
        default=1.0,
        help='检测间隔（秒）(默认: 1.0)'
    )
    monitor_parser.add_argument(
        '-d', '--duration',
        type=float,
        help='监控时长（秒），不指定则持续监控'
    )

    # config 命令
    config_parser = subparsers.add_parser(
        'config',
        help='配置管理'
    )
    config_parser.add_argument(
        '--show',
        action='store_true',
        help='显示当前配置'
    )
    config_parser.add_argument(
        '--reset',
        action='store_true',
        help='重置为默认配置'
    )
    config_parser.add_argument(
        '--set',
        nargs=2,
        metavar=('KEY', 'VALUE'),
        action='append',
        help='设置配置项'
    )

    # test 命令
    subparsers.add_parser(
        'test',
        help='运行自检'
    )

    # gui 命令
    gui_parser = subparsers.add_parser(
        'gui',
        help='GUI界面元素匹配'
    )
    gui_parser.add_argument(
        '-i', '--image',
        type=Path,
        required=True,
        help='输入图像路径'
    )
    gui_parser.add_argument(
        '-t', '--template',
        type=str,
        help='模板文件名（模板匹配模式）'
    )
    gui_parser.add_argument(
        '--text',
        type=str,
        nargs='+',
        help='要识别的文字列表（OCR模式）'
    )
    gui_parser.add_argument(
        '--main-menu',
        action='store_true',
        help='查找主界面按钮'
    )
    gui_parser.add_argument(
        '-o', '--output',
        type=Path,
        help='可视化输出路径'
    )
    gui_parser.add_argument(
        '--threshold',
        type=float,
        default=0.8,
        help='匹配阈值 (默认: 0.8)'
    )

    # main-menu 命令（增强版）
    main_menu_parser = subparsers.add_parser(
        'main-menu',
        help='增强版主界面分析（识别按钮、终端、理智、活动）'
    )
    main_menu_parser.add_argument(
        '-i', '--image',
        type=Path,
        required=True,
        help='主界面截图路径'
    )
    main_menu_parser.add_argument(
        '-o', '--output',
        type=Path,
        help='可视化输出路径'
    )
    main_menu_parser.add_argument(
        '--no-vis',
        action='store_true',
        help='不显示可视化窗口'
    )
    main_menu_parser.add_argument(
        '--debug',
        action='store_true',
        help='保存调试图像（预处理中间结果）'
    )
    main_menu_parser.add_argument(
        '--debug-dir',
        type=Path,
        default='debug_images',
        help='调试图像保存目录 (默认: debug_images)'
    )

    # squad 命令（编队识别）
    squad_parser = subparsers.add_parser(
        'squad',
        help='编队截图识别（识别干员、精英化、等级）'
    )
    squad_parser.add_argument(
        '-i', '--image',
        type=Path,
        required=True,
        help='编队截图路径'
    )
    squad_parser.add_argument(
        '-o', '--output',
        type=Path,
        help='可视化输出路径'
    )
    squad_parser.add_argument(
        '-j', '--json',
        type=Path,
        help='JSON报告输出路径'
    )
    squad_parser.add_argument(
        '--layout',
        choices=['selection', 'edit'],
        default='selection',
        help='布局类型: selection=编队选择(代理作战), edit=编队编辑 (默认: selection)'
    )
    squad_parser.add_argument(
        '--gpu',
        choices=['auto', 'true', 'false'],
        default='auto',
        help='GPU模式: auto=自动检测, true=强制使用, false=强制禁用 (默认: auto)'
    )

    # level 命令（关卡分析）
    level_parser = subparsers.add_parser(
        'level',
        help='关卡地图分析和敌人路径模拟'
    )
    level_parser.add_argument(
        'level_path',
        type=str,
        help='关卡路径（如 "obt/main/level_main_01-07"）'
    )
    level_parser.add_argument(
        '-i', '--info',
        action='store_true',
        help='显示关卡基本信息'
    )
    level_parser.add_argument(
        '-m', '--map',
        type=Path,
        help='输出地图可视化图片路径'
    )
    level_parser.add_argument(
        '-t', '--time',
        type=str,
        help='时间区间（格式: "start,end"，如 "0,30"）'
    )
    level_parser.add_argument(
        '-e', '--enemies',
        type=Path,
        help='输出敌人位置可视化图片路径（需要配合 -t）'
    )
    level_parser.add_argument(
        '--timeline',
        type=Path,
        help='输出敌人时间线图片路径'
    )

    # text 命令（文字定位）
    text_parser = subparsers.add_parser(
        'text',
        help='图像文字定位和点击位置获取'
    )
    text_subparsers = text_parser.add_subparsers(dest='text_command', help='文字子命令')

    # text locate 命令
    locate_parser = text_subparsers.add_parser('locate', help='定位指定文字位置')
    locate_parser.add_argument(
        'image',
        type=Path,
        help='输入图像路径'
    )
    locate_parser.add_argument(
        'query',
        type=str,
        help='要查找的文字'
    )
    locate_parser.add_argument(
        '-m', '--mode',
        choices=['exact', 'partial', 'fuzzy'],
        default='fuzzy',
        help='匹配模式 (默认: fuzzy)'
    )
    locate_parser.add_argument(
        '-t', '--threshold',
        type=float,
        default=0.6,
        help='相似度阈值 (默认: 0.6)'
    )
    locate_parser.add_argument(
        '-o', '--output',
        type=Path,
        help='可视化输出路径'
    )

    # text list 命令
    list_parser = text_subparsers.add_parser('list', help='列出图像中所有文字')
    list_parser.add_argument(
        'image',
        type=Path,
        help='输入图像路径'
    )
    list_parser.add_argument(
        '-o', '--output',
        type=Path,
        help='可视化输出路径'
    )
    list_parser.add_argument(
        '--min-confidence',
        type=float,
        default=0.3,
        help='最小置信度 (默认: 0.3)'
    )

    # data 命令（数据管理）
    data_parser = subparsers.add_parser(
        'data',
        help='明日方舟数据管理（GitHub同步 + PRTS查询）'
    )
    data_subparsers = data_parser.add_subparsers(dest='data_command', help='数据子命令')

    # data sync 命令
    sync_parser = data_subparsers.add_parser('sync', help='同步GitHub数据')
    sync_parser.add_argument(
        '--force',
        action='store_true',
        help='强制同步（即使数据已是最新）'
    )

    # data load 命令
    data_subparsers.add_parser('load', help='加载所有数据到本地数据库')

    # data stats 命令
    data_subparsers.add_parser('stats', help='显示数据统计信息')

    # data operator 命令
    operator_parser = data_subparsers.add_parser('operator', help='查询干员信息')
    operator_parser.add_argument(
        '-i', '--id',
        type=str,
        help='干员ID'
    )
    operator_parser.add_argument(
        '-n', '--name',
        type=str,
        help='干员名称（支持模糊搜索）'
    )

    # data stage 命令
    stage_parser = data_subparsers.add_parser('stage', help='查询关卡信息')
    stage_parser.add_argument(
        '-i', '--id',
        type=str,
        help='关卡ID'
    )
    stage_parser.add_argument(
        '-c', '--code',
        type=str,
        help='关卡代码（如 1-7）'
    )

    # data item 命令
    item_parser = data_subparsers.add_parser('item', help='查询物品信息')
    item_parser.add_argument(
        '-i', '--id',
        type=str,
        help='物品ID'
    )
    item_parser.add_argument(
        '-n', '--name',
        type=str,
        help='物品名称（支持模糊搜索）'
    )

    # data search 命令（PRTS搜索）
    search_parser = data_subparsers.add_parser('search', help='在PRTS Wiki搜索')
    search_parser.add_argument(
        'query',
        type=str,
        help='搜索关键词'
    )
    search_parser.add_argument(
        '-l', '--limit',
        type=int,
        default=10,
        help='返回结果数量限制 (默认: 10)'
    )

    # data page 命令（PRTS页面）
    page_parser = data_subparsers.add_parser('page', help='获取PRTS Wiki页面内容')
    page_parser.add_argument(
        'title',
        type=str,
        help='页面标题'
    )

    # data load-structured 命令
    data_subparsers.add_parser('load-structured', help='加载所有数据到结构化数据库（支持复杂查询）')

    # data query 命令（结构化查询）
    query_parser = data_subparsers.add_parser('query', help='使用结构化数据库查询')
    query_parser.add_argument(
        'entity_type',
        choices=['operator', 'stage', 'item', 'enemy'],
        help='实体类型'
    )
    query_parser.add_argument(
        '--profession',
        type=str,
        help='干员职业筛选（如: SNIPER, CASTER）'
    )
    query_parser.add_argument(
        '--rarity',
        type=str,
        help='稀有度筛选（如: 6, 5, 4）'
    )
    query_parser.add_argument(
        '--min-rarity',
        type=str,
        help='最小稀有度'
    )
    query_parser.add_argument(
        '--max-rarity',
        type=str,
        help='最大稀有度'
    )
    query_parser.add_argument(
        '--stage-type',
        type=str,
        help='关卡类型（如: MAIN, ACTIVITY）'
    )
    query_parser.add_argument(
        '--zone-id',
        type=str,
        help='区域ID'
    )
    query_parser.add_argument(
        '--difficulty',
        type=str,
        help='难度（如: NORMAL, HARD）'
    )
    query_parser.add_argument(
        '--min-ap-cost',
        type=str,
        help='最小理智消耗'
    )
    query_parser.add_argument(
        '--max-ap-cost',
        type=str,
        help='最大理智消耗'
    )
    query_parser.add_argument(
        '--item-type',
        type=str,
        help='物品类型（如: MATERIAL, CARD_EXP）'
    )
    query_parser.add_argument(
        '--is-material',
        type=str,
        help='是否为材料（true/false）'
    )
    query_parser.add_argument(
        '--is-exp-card',
        type=str,
        help='是否为经验卡（true/false）'
    )
    query_parser.add_argument(
        '--enemy-level',
        type=str,
        help='敌人等级（如: NORMAL, ELITE, BOSS）'
    )
    query_parser.add_argument(
        '--min-hp',
        type=str,
        help='最小生命值'
    )
    query_parser.add_argument(
        '--max-hp',
        type=str,
        help='最大生命值'
    )
    query_parser.add_argument(
        '--min-atk',
        type=str,
        help='最小攻击力'
    )
    query_parser.add_argument(
        '--max-atk',
        type=str,
        help='最大攻击力'
    )
    query_parser.add_argument(
        '--nation-id',
        type=str,
        help='国家ID（干员筛选）'
    )
    query_parser.add_argument(
        '--team-id',
        type=str,
        help='团队ID（干员筛选）'
    )
    query_parser.add_argument(
        '--is-robot',
        type=str,
        help='是否为机器人（true/false）'
    )
    query_parser.add_argument(
        '--can-practice',
        type=str,
        help='是否可演习（true/false）'
    )
    query_parser.add_argument(
        '-l', '--limit',
        type=str,
        default='20',
        help='返回数量限制 (默认: 20)'
    )

    # data material-tree 命令
    material_tree_parser = data_subparsers.add_parser('material-tree', help='获取材料合成树')
    material_tree_parser.add_argument(
        'item_id',
        type=str,
        help='物品ID'
    )

    return parser


# =============================================================================
# 主函数
# =============================================================================

def main():
    """主入口"""
    parser = create_parser()
    args = parser.parse_args()

    # 加载配置
    cli_config = CLIConfig.load(args.config)
    cli_config.log_level = args.log_level
    cli_config.enable_gpu = not args.no_gpu

    # 设置日志
    log_file = DEFAULT_LOG_PATH / f"detector_{datetime.now():%Y%m%d}.log"
    logger = setup_logging(
        level=args.log_level,
        log_file=log_file,
        console=True
    )

    logger.info(f"{APP_NAME} v{APP_VERSION}")

    # 检查EasyOCR
    if not EASYOCR_AVAILABLE:
        logger.error("EasyOCR未安装，请执行: pip install easyocr")
        return 1

    # 执行命令
    commands = DetectorCommands(cli_config, logger)

    try:
        if args.command == 'detect':
            if args.image:
                result = commands.detect_single(
                    args.image,
                    visualize=args.visualize,
                    save_roi=args.save_roi
                )

                # 自动保存
                if cli_config.auto_save and cli_config.save_dir:
                    output = Path(cli_config.save_dir) / f"{args.image.stem}_result.json"
                    with open(output, 'w') as f:
                        json.dump(result.to_dict(), f, indent=2)

                return 0

            elif args.batch:
                format_map = {
                    'json': OutputFormat.JSON,
                    'csv': OutputFormat.CSV,
                    'txt': OutputFormat.TXT,
                    'console': OutputFormat.CONSOLE
                }
                commands.detect_batch(
                    args.batch,
                    output=args.output,
                    format=format_map[args.format],
                    recursive=args.recursive
                )
                return 0
            else:
                detect_parser = [a for a in parser._actions if isinstance(a, argparse._SubParsersAction)][0].choices['detect']
                detect_parser.print_help()
                return 0

        elif args.command == 'monitor':
            commands.monitor(
                interval=args.interval,
                duration=args.duration
            )

        elif args.command == 'config':
            if args.show:
                print("当前配置:")
                print(json.dumps(asdict(cli_config), indent=2, ensure_ascii=False))
            elif args.reset:
                cli_config = CLIConfig()
                cli_config.save(args.config)
                print("配置已重置")
            elif args.set:
                for key, value in args.set:
                    # 简单类型转换
                    if value.lower() in ('true', 'false'):
                        value = value.lower() == 'true'
                    elif value.isdigit():
                        value = int(value)
                    elif value.replace('.', '').isdigit():
                        value = float(value)

                    if hasattr(cli_config, key):
                        setattr(cli_config, key, value)
                        print(f"设置 {key} = {value}")
                    else:
                        print(f"未知配置项: {key}")
                cli_config.save(args.config)
            else:
                config_parser = [a for a in parser._actions if isinstance(a, argparse._SubParsersAction)][0].choices['config']
                config_parser.print_help()

        elif args.command == 'test':
            success = commands.test()
            return 0 if success else 1

        elif args.command == 'gui':
            success = commands.gui_match(
                image_path=args.image,
                template=args.template,
                texts=args.text,
                main_menu=args.main_menu,
                output=args.output,
                threshold=args.threshold
            )
            return 0 if success else 1

        elif args.command == 'main-menu':
            success = commands.analyze_main_menu(
                image_path=args.image,
                output=args.output,
                show_vis=not args.no_vis,
                debug=args.debug,
                debug_dir=args.debug_dir
            )
            return 0 if success else 1

        elif args.command == 'squad':
            # 根据布局类型选择配置
            if args.layout == 'edit':
                config = SquadConfig.preset_squad_edit()
            else:
                config = SquadConfig.preset_squad_selection()

            # 设置GPU配置
            gpu_config = args.gpu
            if gpu_config == 'true':
                config.ocr_gpu = True
            elif gpu_config == 'false':
                config.ocr_gpu = False
            else:
                config.ocr_gpu = 'auto'

            # 创建分析器
            analyzer = SquadAnalyzer(config)

            if not analyzer.initialize():
                print("初始化失败")
                return 1

            try:
                print(f"\n分析编队截图: {args.image}")
                print("="*70)

                # 分析编队
                result = analyzer.analyze(args.image)

                # 打印报告
                analyzer.print_report(result)

                # 打印待部署区列表
                analyzer.print_deploy_list(result)

                # 保存可视化
                if args.output:
                    analyzer.visualize(args.image, result, args.output)
                    print(f"\n可视化结果已保存: {args.output}")

                # 保存JSON
                if args.json:
                    import json
                    with open(args.json, 'w', encoding='utf-8') as f:
                        json.dump(result.to_dict(), f, ensure_ascii=False, indent=2)
                    print(f"JSON报告已保存: {args.json}")

            finally:
                analyzer.shutdown()

            return 0

        elif args.command == 'level':
            from src.map import LevelAnalyzer, MapVisualizer

            # 创建分析器
            level_analyzer = LevelAnalyzer()

            # 加载关卡
            level_data = level_analyzer.load_level(args.level_path)
            if not level_data:
                print(f"加载关卡失败: {args.level_path}")
                return 1

            # 显示基本信息
            if args.info:
                summary = level_analyzer.get_level_summary()
                print("\n" + "=" * 70)
                print("关卡信息")
                print("=" * 70)
                print(f"关卡ID: {summary.get('level_id', 'N/A')}")
                print(f"地图大小: {summary.get('map_size', 'N/A')}")
                print(f"干员限制: {summary.get('character_limit', 'N/A')}")
                print(f"生命值: {summary.get('max_life_point', 'N/A')}")
                print(f"初始费用: {summary.get('initial_cost', 'N/A')}")
                print(f"路径数量: {summary.get('routes_count', 'N/A')}")
                print(f"波次数量: {summary.get('waves_count', 'N/A')}")
                print(f"敌人类型: {', '.join(summary.get('enemy_types', []))}")
                print(f"敌人总数: {summary.get('total_enemies', 'N/A')}")
                print(f"出生点: {summary.get('start_positions', [])}")
                print(f"终点: {summary.get('end_positions', [])}")
                print("=" * 70)

            # 创建可视化器
            visualizer = MapVisualizer()

            # 输出地图
            if args.map:
                visualizer.visualize_map(level_data, output_path=args.map)
                print(f"地图已保存: {args.map}")

            # 时间区间分析
            if args.time:
                try:
                    start_time, end_time = map(float, args.time.split(','))
                    enemies = level_analyzer.get_enemies_in_time_range(start_time, end_time)

                    print(f"\n时间区间 [{start_time}s, {end_time}s] 内的敌人:")
                    print("-" * 70)
                    for enemy in enemies:
                        pos = enemy.get('position')
                        pos_str = f"({pos.row}, {pos.col})" if pos else "未知"
                        print(f"  {enemy['enemy_key']}: 生成时间={enemy['spawn_time']:.1f}s, 位置={pos_str}")
                    print("-" * 70)
                    print(f"总计: {len(enemies)} 个敌人")

                    # 输出敌人位置可视化
                    if args.enemies:
                        visualizer.visualize_map(level_data, enemies=enemies, output_path=args.enemies)
                        print(f"敌人位置图已保存: {args.enemies}")

                except ValueError:
                    print("错误: 时间格式不正确，请使用 'start,end' 格式（如 '0,30'）")
                    return 1

            # 输出时间线
            if args.timeline:
                if args.time:
                    start_time, end_time = map(float, args.time.split(','))
                else:
                    # 自动计算时间范围
                    start_time = 0.0
                    end_time = 60.0  # 默认60秒

                visualizer.visualize_enemy_timeline(
                    level_data,
                    (start_time, end_time),
                    output_path=args.timeline
                )
                print(f"敌人时间线已保存: {args.timeline}")

            return 0

        elif args.command == 'text':
            from src.vision.text_locator import TextLocator

            if args.text_command == 'locate':
                # 定位指定文字
                locator = TextLocator(confidence_threshold=0.3)
                if not locator.initialize():
                    print("初始化OCR引擎失败")
                    return 1

                try:
                    result = locator.locate_text(
                        args.image,
                        args.query,
                        match_mode=args.mode,
                        similarity_threshold=args.threshold
                    )

                    if result and result.region:
                        print("\n" + "=" * 70)
                        print("文字定位结果")
                        print("=" * 70)
                        print(f"查询文字: {result.query}")
                        print(f"匹配文字: {result.matched_text}")
                        print(f"匹配类型: {result.match_type}")
                        print(f"相似度: {result.similarity:.2%}")
                        print(f"置信度: {result.region.confidence:.2%}")
                        print(f"\n点击位置 (x, y): ({result.region.center[0]}, {result.region.center[1]})")
                        print(f"边界框: {result.region.bbox}")
                        print("=" * 70)

                        # 可视化
                        if args.output:
                            locator.visualize_text_locations(
                                args.image,
                                output_path=args.output,
                                highlight_queries=[args.query]
                            )
                            print(f"可视化结果已保存: {args.output}")

                        return 0
                    else:
                        print(f"未找到文字: '{args.query}'")
                        return 1

                finally:
                    locator.shutdown()

            elif args.text_command == 'list':
                # 列出所有文字
                locator = TextLocator(confidence_threshold=args.min_confidence)
                if not locator.initialize():
                    print("初始化OCR引擎失败")
                    return 1

                try:
                    regions = locator.detect_text(args.image)

                    print("\n" + "=" * 70)
                    print(f"检测到的文字 (共 {len(regions)} 个)")
                    print("=" * 70)
                    print(f"{'序号':<6}{'文字':<30}{'位置(x,y)':<15}{'置信度':<10}")
                    print("-" * 70)

                    for i, region in enumerate(regions, 1):
                        text = region.text[:28] + '..' if len(region.text) > 30 else region.text
                        print(f"{i:<6}{text:<30}({region.center[0]:4d},{region.center[1]:4d}){region.confidence:>9.1%}")

                    print("=" * 70)

                    # 可视化
                    if args.output:
                        locator.visualize_text_locations(args.image, output_path=args.output)
                        print(f"可视化结果已保存: {args.output}")

                    return 0

                finally:
                    locator.shutdown()

            else:
                text_parser = [a for a in parser._actions if isinstance(a, argparse._SubParsersAction)][0].choices['text']
                text_parser.print_help()

            return 0

        elif args.command == 'data':
            data_commands = DataCommands(logger)

            if args.data_command == 'sync':
                success = data_commands.sync(force=args.force)
                return 0 if success else 1

            elif args.data_command == 'load':
                success = data_commands.load()
                return 0 if success else 1

            elif args.data_command == 'stats':
                success = data_commands.stats()
                return 0 if success else 1

            elif args.data_command == 'operator':
                success = data_commands.query_operator(
                    operator_id=args.id,
                    name=args.name
                )
                return 0 if success else 1

            elif args.data_command == 'stage':
                success = data_commands.query_stage(
                    stage_id=args.id,
                    code=args.code
                )
                return 0 if success else 1

            elif args.data_command == 'item':
                success = data_commands.query_item(
                    item_id=args.id,
                    name=args.name
                )
                return 0 if success else 1

            elif args.data_command == 'search':
                success = data_commands.search_prts(
                    query=args.query,
                    limit=args.limit
                )
                return 0 if success else 1

            elif args.data_command == 'page':
                success = data_commands.get_prts_page(
                    title=args.title
                )
                return 0 if success else 1

            elif args.data_command == 'load-structured':
                success = data_commands.load_structured()
                return 0 if success else 1

            elif args.data_command == 'query':
                # 收集所有非None的参数
                query_params = {}
                for key, value in vars(args).items():
                    if value is not None and key not in ['command', 'data_command', 'entity_type', 'config', 'log_level', 'no_gpu']:
                        query_params[key] = value

                success = data_commands.query_structured(
                    entity_type=args.entity_type,
                    **query_params
                )
                return 0 if success else 1

            elif args.data_command == 'material-tree':
                success = data_commands.get_material_tree(
                    item_id=args.item_id
                )
                return 0 if success else 1

            else:
                data_parser = [a for a in parser._actions if isinstance(a, argparse._SubParsersAction)][0].choices['data']
                data_parser.print_help()

        else:
            parser.print_help()

    except KeyboardInterrupt:
        logger.info("用户中断")
        return 130
    except Exception as e:
        logger.error(f"错误: {e}")
        import traceback
        traceback.print_exc()
        return 1
    finally:
        commands._release_detector()

    return 0


if __name__ == '__main__':
    sys.exit(main())
