# -*- coding: utf-8 -*-
"""
干员匹配模块

提供干员名称的模糊匹配和距离搜索功能
当OCR识别置信度低时，使用编辑距离进行模糊匹配

Author: Data System
Version: 1.0.0
"""

import logging
from typing import List, Optional, Tuple, Dict, Any
from dataclasses import dataclass
from difflib import SequenceMatcher
import re

from .models.operator import Operator
from .providers.data_manager import DataManager

logger = logging.getLogger(__name__)


@dataclass
class MatchResult:
    """匹配结果"""
    operator: Operator
    match_score: float  # 匹配分数 (0-1)
    match_type: str     # 匹配类型: 'exact', 'partial', 'fuzzy'
    matched_name: str   # 实际匹配到的名称

    def to_dict(self) -> Dict[str, Any]:
        return {
            'operator_id': self.operator.id,
            'operator_name': self.operator.name,
            'appellation': self.operator.appellation,
            'match_score': self.match_score,
            'match_type': self.match_type,
            'matched_name': self.matched_name,
            'rarity': self.operator.stars,
            'profession': self.operator.profession.value,
            'sub_profession': self.operator.sub_profession_id,
            'position': self.operator.position.value,
        }


class OperatorMatcher:
    """
    干员匹配器

    提供多种匹配策略：
    1. 精确匹配：名称完全匹配
    2. 部分匹配：包含关系匹配
    3. 模糊匹配：基于编辑距离的相似度匹配
    """

    # 常见OCR错误映射
    OCR_ERROR_PATTERNS = {
        '壬': '王' # 后续更新
    }

    def __init__(self, data_manager: Optional[DataManager] = None):
        """
        初始化匹配器

        Args:
            data_manager: 数据管理器实例
        """
        self._data_manager = data_manager
        self._operators: List[Operator] = []
        self._name_index: Dict[str, Operator] = {}  # 名称 -> 干员
        self._alias_index: Dict[str, str] = {}      # 别名 -> 标准名称

    def initialize(self) -> bool:
        """
        初始化匹配器

        Returns:
            是否初始化成功
        """
        try:
            logger.info("初始化干员匹配器...")

            # 获取所有干员
            if self._data_manager:
                self._operators = self._data_manager.get_operators()
            else:
                # 创建新的数据管理器
                self._data_manager = DataManager()
                if not self._data_manager.initialize():
                    logger.error("数据管理器初始化失败")
                    return False
                self._operators = self._data_manager.get_operators()

            # 构建名称索引
            self._build_name_index()

            logger.info(f"干员匹配器初始化完成，共加载 {len(self._operators)} 个干员")
            return True

        except Exception as e:
            logger.error(f"初始化失败: {e}")
            return False

    def _build_name_index(self) -> None:
        """构建名称索引"""
        self._name_index = {}
        self._alias_index = {}

        for op in self._operators:
            # 主名称
            if op.name:
                self._name_index[op.name] = op
                # 添加简体/繁体变体
                simplified = self._to_simplified(op.name)
                if simplified != op.name:
                    self._name_index[simplified] = op

            # 英文名/代号
            if op.appellation:
                self._name_index[op.appellation] = op

            # 处理异格干员（名称通常包含前缀）
            if op.is_sp_char and op.name:
                # 尝试提取基础名称
                base_name = self._extract_base_name(op.name)
                if base_name and base_name != op.name:
                    self._alias_index[base_name] = op.name

    def _to_simplified(self, text: str) -> str:
        """
        将繁体中文转换为简体中文

        Args:
            text: 输入文本

        Returns:
            简体中文文本
        """
        # 常见繁简转换映射
        traditional_map = {
            '幹': '干', '乾': '干', '亁': '干',
            '陳': '陈',
            '國': '国',
            '長': '长',
            '門': '门',
            '馬': '马',
            '風': '风',
            '車': '车',
            '東': '东',
            '無': '无',
            '見': '见',
            '時': '时',
            '從': '从',
            '來': '来',
            '個': '个',
            '們': '们',
            '說': '说',
            '過': '过',
            '這': '这',
            '為': '为',
            '與': '与',
            '進': '进',
            '還': '还',
            '讓': '让',
            '對': '对',
            '產': '产',
            '實': '实',
            '學': '学',
            '問': '问',
            '開': '开',
            '關': '关',
            '後': '后',
            '應': '应',
            '頭': '头',
            '點': '点',
            '員': '员',
            '業': '业',
            '務': '务',
            '醫': '医',
            '師': '师',
            '衛': '卫',
            '隊': '队',
            '戰': '战',
            '鬥': '斗',
            '術': '术',
            '輔': '辅',
            '導': '导',
            '獵': '猎',
            '殺': '杀',
            '擊': '击',
        }

        result = []
        for char in text:
            result.append(traditional_map.get(char, char))
        return ''.join(result)

    def _extract_base_name(self, name: str) -> Optional[str]:
        """
        从异格干员名称中提取基础名称

        Args:
            name: 干员名称

        Returns:
            基础名称或None
        """
        # 常见异格前缀
        sp_prefixes = ['浊心', '假日威龙', '耀骑士', '归溟', '缄默', '百炼', '纯烬',
                       '涤火', '重岳', '麒麟R夜刀']

        for prefix in sp_prefixes:
            if name.startswith(prefix):
                return name[len(prefix):]

        return None

    def _normalize_text(self, text: str) -> str:
        """
        标准化文本

        Args:
            text: 输入文本

        Returns:
            标准化后的文本
        """
        # 移除空格和特殊字符
        text = re.sub(r'\s+', '', text)
        # 转换为简体中文
        text = self._to_simplified(text)
        # 修复常见OCR错误
        for wrong, correct in self.OCR_ERROR_PATTERNS.items():
            text = text.replace(wrong, correct)
        return text

    def _calculate_similarity(self, s1: str, s2: str) -> float:
        """
        计算两个字符串的相似度

        Args:
            s1: 字符串1
            s2: 字符串2

        Returns:
            相似度分数 (0-1)
        """
        # 使用SequenceMatcher计算相似度
        return SequenceMatcher(None, s1, s2).ratio()

    def match(
        self,
        query: str,
        threshold: float = 0.6,
        max_results: int = 3,
        use_fuzzy: bool = True
    ) -> List[MatchResult]:
        """
        匹配干员

        Args:
            query: 查询名称
            threshold: 匹配阈值 (0-1)
            max_results: 最大返回结果数
            use_fuzzy: 是否使用模糊匹配

        Returns:
            匹配结果列表
        """
        if not query or not self._operators:
            return []

        normalized_query = self._normalize_text(query)
        results: List[MatchResult] = []

        # 1. 精确匹配
        if normalized_query in self._name_index:
            op = self._name_index[normalized_query]
            results.append(MatchResult(
                operator=op,
                match_score=1.0,
                match_type='exact',
                matched_name=op.name
            ))
            return results

        # 2. 部分匹配（包含关系）
        for name, op in self._name_index.items():
            normalized_name = self._normalize_text(name)
            if normalized_query in normalized_name or normalized_name in normalized_query:
                # 计算包含匹配分数
                len_ratio = min(len(normalized_query), len(normalized_name)) / max(len(normalized_query), len(normalized_name))
                score = 0.7 + 0.3 * len_ratio  # 基础分0.7 + 长度比例

                # 检查是否已存在
                existing = next((r for r in results if r.operator.id == op.id), None)
                if existing:
                    if score > existing.match_score:
                        existing.match_score = score
                        existing.match_type = 'partial'
                else:
                    results.append(MatchResult(
                        operator=op,
                        match_score=score,
                        match_type='partial',
                        matched_name=name
                    ))

        # 3. 模糊匹配
        if use_fuzzy and len(results) < max_results:
            for name, op in self._name_index.items():
                # 跳过已匹配的结果
                if any(r.operator.id == op.id for r in results):
                    continue

                normalized_name = self._normalize_text(name)
                similarity = self._calculate_similarity(normalized_query, normalized_name)

                if similarity >= threshold:
                    results.append(MatchResult(
                        operator=op,
                        match_score=similarity,
                        match_type='fuzzy',
                        matched_name=name
                    ))

        # 按分数排序并限制结果数
        results.sort(key=lambda x: x.match_score, reverse=True)
        return results[:max_results]

    def match_single(
        self,
        query: str,
        threshold: float = 0.6,
        use_fuzzy: bool = True
    ) -> Optional[MatchResult]:
        """
        匹配单个干员（返回最佳匹配）

        Args:
            query: 查询名称
            threshold: 匹配阈值
            use_fuzzy: 是否使用模糊匹配

        Returns:
            最佳匹配结果或None
        """
        results = self.match(query, threshold, max_results=1, use_fuzzy=use_fuzzy)
        return results[0] if results else None

    def get_operator_info(
        self,
        operator_name: str,
        elite_level: int = 0,
        level: int = 1,
        confidence: float = 0.0,
        confidence_threshold: float = 0.7
    ) -> Dict[str, Any]:
        """
        获取干员详细信息

        Args:
            operator_name: 干员名称
            elite_level: 精英化等级
            level: 等级
            confidence: 识别置信度
            confidence_threshold: 置信度阈值，低于此值使用模糊匹配

        Returns:
            干员信息字典
        """
        result = {
            'name': operator_name,
            'elite_level': elite_level,
            'level': level,
            'confidence': confidence,
            'matched': False,
            'match_score': 0.0,
            'match_type': 'none',
            'operator_id': None,
            'rarity': None,
            'profession': None,
            'sub_profession': None,
            'position': None,
            'cost': None,
            'phase_info': None,
            'alternatives': []
        }

        # 决定是否使用模糊匹配
        use_fuzzy = confidence < confidence_threshold

        # 匹配干员
        match_result = self.match_single(operator_name, use_fuzzy=use_fuzzy)

        if match_result:
            op = match_result.operator
            result['matched'] = True
            result['match_score'] = match_result.match_score
            result['match_type'] = match_result.match_type
            result['operator_id'] = op.id
            result['name'] = op.name  # 使用标准名称
            result['rarity'] = op.stars
            result['profession'] = op.profession.value
            result['sub_profession'] = op.sub_profession_id
            result['position'] = op.position.value

            # 获取当前精英化阶段的属性
            phase = op.get_phase(elite_level)
            if phase:
                result['cost'] = phase.cost
                result['phase_info'] = {
                    'max_hp': phase.max_hp,
                    'atk': phase.atk,
                    'def': phase.def_,
                    'magic_resistance': phase.magic_resistance,
                    'cost': phase.cost,
                    'block_count': phase.block_count
                }
            else:
                # 使用默认费用
                result['cost'] = 20

            # 如果使用了模糊匹配，添加备选结果
            if use_fuzzy and match_result.match_type == 'fuzzy':
                alternatives = self.match(operator_name, max_results=3, use_fuzzy=True)
                result['alternatives'] = [
                    {
                        'name': alt.operator.name,
                        'score': alt.match_score,
                        'type': alt.match_type
                    }
                    for alt in alternatives[1:]  # 排除第一个（已经是最佳匹配）
                    if alt.match_score >= 0.5
                ]

        return result

    def batch_match(
        self,
        operators: List[Dict[str, Any]],
        confidence_threshold: float = 0.7
    ) -> List[Dict[str, Any]]:
        """
        批量匹配干员

        Args:
            operators: 干员信息列表，每个包含name, elite_level, level, confidence
            confidence_threshold: 置信度阈值

        Returns:
            匹配结果列表
        """
        results = []
        for op_info in operators:
            info = self.get_operator_info(
                operator_name=op_info.get('name', ''),
                elite_level=op_info.get('elite_level', 0),
                level=op_info.get('level', 1),
                confidence=op_info.get('confidence', 0.0),
                confidence_threshold=confidence_threshold
            )
            results.append(info)
        return results
