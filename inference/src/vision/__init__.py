# -*- coding: utf-8 -*-
"""
视觉识别模块 - 明日方舟自动化系统

包含以下子模块：
- game_state_detector: 游戏状态检测（对局/非对局）
- detector: 核心检测器（干员、敌人识别）
- ui_detector: UI信息检测
- colors: 颜色配置
- entities: 实体定义

使用示例：
    >>> from vision import GameStateDetector, GameState
    >>> detector = GameStateDetector()
    >>> detector.initialize()
    >>> result = detector.detect(image)
    >>> if result.state == GameState.IN_BATTLE:
    ...     print("当前在对局中")
"""

from .game_state_detector import (
    GameStateDetector,
    DetectorConfig,
    DetectionResult,
    GameState,
    ImagePreprocessor,
    TextMatcher,
    detect_game_state,
    create_default_detector,
    EASYOCR_AVAILABLE,
)

__all__ = [
    'GameStateDetector',
    'DetectorConfig',
    'DetectionResult',
    'GameState',
    'ImagePreprocessor',
    'TextMatcher',
    'detect_game_state',
    'create_default_detector',
    'EASYOCR_AVAILABLE',
]

__version__ = '1.0.0'
