# -*- coding: utf-8 -*-
"""
编队分析器

整合编队识别和数据库查询，提供完整的编队分析功能

Author: Vision System
Version: 1.0.0
"""

import cv2
import numpy as np
import logging
from pathlib import Path
from typing import List, Optional, Dict, Any, Tuple
from dataclasses import dataclass, field
from datetime import datetime

from .squad_recognizer import SquadRecognizer, SquadConfig, OperatorCard
from data import DataManager, ManagerConfig
from data.operator_matcher import OperatorMatcher

logger = logging.getLogger(__name__)


@dataclass
class SquadAnalysisResult:
    """编队分析结果"""
    # 基本信息
    image_path: Path
    analyzed_at: datetime = field(default_factory=datetime.now)

    # 识别的干员（原始顺序）
    operators: List[OperatorCard] = field(default_factory=list)

    # 排序后的干员列表（用于待部署区显示）
    sorted_operators: List[OperatorCard] = field(default_factory=list)

    # 统计信息
    total_operators: int = 0
    elite_distribution: Dict[int, int] = field(default_factory=dict)
    average_level: float = 0.0
    total_cost: int = 0

    # 职业分布
    profession_distribution: Dict[str, int] = field(default_factory=dict)

    # 星级分布
    rarity_distribution: Dict[int, int] = field(default_factory=dict)

    def to_dict(self) -> Dict[str, Any]:
        """转换为字典"""
        return {
            'image_path': str(self.image_path),
            'analyzed_at': self.analyzed_at.isoformat(),
            'total_operators': self.total_operators,
            'average_level': self.average_level,
            'total_cost': self.total_cost,
            'elite_distribution': self.elite_distribution,
            'profession_distribution': self.profession_distribution,
            'rarity_distribution': self.rarity_distribution,
            'operators': [
                {
                    'name': op.name,
                    'elite_level': op.elite_level.value,
                    'level': op.level,
                    'position': op.grid_position,
                    'confidence': op.name_confidence,
                    'info': op.operator_info
                }
                for op in self.operators
            ],
            'sorted_operators': [
                {
                    'name': op.name,
                    'elite_level': op.elite_level.value,
                    'level': op.level,
                    'cost': op.operator_info.get('cost', 0) if op.operator_info else 0,
                    'profession': op.operator_info.get('profession', 'Unknown') if op.operator_info else 'Unknown',
                    'operator_id': op.operator_info.get('id', '') if op.operator_info else '',
                    'stars': op.operator_info.get('stars', 0) if op.operator_info else 0,
                }
                for op in self.sorted_operators
            ]
        }


class SquadAnalyzer:
    """
    编队分析器

    整合编队识别和数据库查询功能
    """

    def __init__(
        self,
        squad_config: Optional[SquadConfig] = None,
        data_config: Optional[ManagerConfig] = None
    ):
        """
        初始化编队分析器

        Args:
            squad_config: 编队识别配置
            data_config: 数据管理器配置
        """
        self.squad_config = squad_config or SquadConfig()
        self.data_config = data_config or ManagerConfig()

        self._recognizer: Optional[SquadRecognizer] = None
        self._data_manager: Optional[DataManager] = None
        self._operator_matcher: Optional[OperatorMatcher] = None
        self._confidence_threshold: float = 0.7  # 置信度阈值，低于此值使用模糊匹配
        self._initialized = False

    def initialize(self) -> bool:
        """
        初始化分析器

        Returns:
            是否初始化成功
        """
        try:
            logger.info("初始化编队分析器...")

            # 初始化编队识别器
            self._recognizer = SquadRecognizer(self.squad_config)
            if not self._recognizer.initialize():
                logger.error("编队识别器初始化失败")
                return False

            # 初始化数据管理器
            self._data_manager = DataManager(self.data_config)
            if not self._data_manager.initialize():
                logger.warning("数据管理器初始化失败，将只进行基础识别")
                # 继续，因为基础识别仍然可以工作
            else:
                # 加载干员数据
                logger.info("加载干员数据...")
                if not self._data_manager.load_all_data():
                    logger.warning("加载干员数据失败")
                else:
                    # 初始化干员匹配器
                    self._operator_matcher = OperatorMatcher(self._data_manager)
                    if not self._operator_matcher.initialize():
                        logger.warning("干员匹配器初始化失败")
                        self._operator_matcher = None

            self._initialized = True
            logger.info("编队分析器初始化完成")
            return True

        except Exception as e:
            logger.error(f"初始化失败: {e}")
            return False

    def analyze(
        self,
        image_path: Path,
        query_database: bool = True
    ) -> Optional[SquadAnalysisResult]:
        """
        分析编队截图

        Args:
            image_path: 编队截图路径
            query_database: 是否查询数据库获取详细信息

        Returns:
            分析结果或None
        """
        if not self._initialized:
            raise RuntimeError("分析器未初始化")

        # 加载图像
        image = cv2.imread(str(image_path))
        if image is None:
            logger.error(f"无法加载图像: {image_path}")
            return None

        logger.info(f"分析编队截图: {image_path}")

        # 识别干员卡片
        cards = self._recognizer.recognize_squad(image)

        if not cards:
            logger.warning("未识别到任何干员")
            return None

        # 查询数据库获取详细信息
        if query_database and self._data_manager:
            self._query_operator_info(cards)

        # 生成分析结果
        result = self._generate_result(image_path, cards)

        return result

    def _query_operator_info(self, cards: List[OperatorCard]) -> None:
        """
        查询干员详细信息

        使用干员匹配器进行智能匹配，当OCR置信度低时使用模糊匹配

        Args:
            cards: 干员卡片列表
        """
        if not self._operator_matcher:
            logger.warning("干员匹配器未初始化，跳过数据库查询")
            return

        for card in cards:
            try:
                # 使用干员匹配器获取详细信息
                op_info = self._operator_matcher.get_operator_info(
                    operator_name=card.name,
                    elite_level=card.elite_level.value,
                    level=card.level,
                    confidence=card.name_confidence,
                    confidence_threshold=self._confidence_threshold
                )

                if op_info['matched']:
                    # 更新卡片名称（使用标准名称）
                    if op_info['match_type'] in ['exact', 'partial'] or op_info['match_score'] >= 0.8:
                        card.name = op_info['name']

                    # 构建干员信息
                    card.operator_info = {
                        'id': op_info['operator_id'],
                        'name': op_info['name'],
                        'rarity': op_info['rarity'],
                        'stars': op_info['rarity'],
                        'profession': op_info['profession'],
                        'sub_profession': op_info['sub_profession'],
                        'position': op_info['position'],
                        'cost': op_info['cost'],
                        'phase_info': op_info['phase_info'],
                        'match_score': op_info['match_score'],
                        'match_type': op_info['match_type'],
                    }

                    # 如果有备选结果，也添加进去
                    if op_info['alternatives']:
                        card.operator_info['alternatives'] = op_info['alternatives']

                    logger.debug(
                        f"查询到干员信息: {op_info['name']} "
                        f"(匹配类型: {op_info['match_type']}, 分数: {op_info['match_score']:.2f})"
                    )
                else:
                    logger.debug(f"未找到干员信息: {card.name}")
                    card.operator_info = {
                        'match_score': 0.0,
                        'match_type': 'none',
                        'alternatives': op_info.get('alternatives', [])
                    }

            except Exception as e:
                logger.warning(f"查询干员信息失败 {card.name}: {e}")

    def _generate_result(
        self,
        image_path: Path,
        cards: List[OperatorCard]
    ) -> SquadAnalysisResult:
        """
        生成分析结果

        Args:
            image_path: 图像路径
            cards: 干员卡片列表

        Returns:
            分析结果
        """
        result = SquadAnalysisResult(
            image_path=image_path,
            operators=cards,
            total_operators=len(cards)
        )

        # 精英化分布
        elite_dist = {0: 0, 1: 0, 2: 0}
        for card in cards:
            elite_dist[card.elite_level.value] = elite_dist.get(card.elite_level.value, 0) + 1
        result.elite_distribution = elite_dist

        # 平均等级
        if cards:
            result.average_level = sum(card.level for card in cards) / len(cards)

        # 职业分布和星级分布
        profession_dist = {}
        rarity_dist = {}
        total_cost = 0

        for card in cards:
            if card.operator_info:
                # 职业
                prof = card.operator_info.get('profession', 'Unknown')
                profession_dist[prof] = profession_dist.get(prof, 0) + 1

                # 星级
                rarity = card.operator_info.get('stars', 0)
                rarity_dist[rarity] = rarity_dist.get(rarity, 0) + 1

                # 费用（从phase_info获取）
                cost = card.operator_info.get('cost', 0)
                if cost and cost > 0:
                    total_cost += cost
                else:
                    # 使用估算值
                    base_cost = 10 + (rarity - 3) * 2 if rarity > 0 else 15
                    total_cost += base_cost

        result.profession_distribution = profession_dist
        result.rarity_distribution = rarity_dist
        result.total_cost = total_cost

        # 按待部署区规则排序干员
        result.sorted_operators = self._sort_operators_for_deploy(cards)

        return result

    def print_report(self, result: SquadAnalysisResult) -> None:
        """
        打印分析报告

        Args:
            result: 分析结果
        """
        print("\n" + "=" * 70)
        print("编队分析报告")
        print("=" * 70)
        print(f"图像: {result.image_path}")
        print(f"分析时间: {result.analyzed_at.strftime('%Y-%m-%d %H:%M:%S')}")
        print("-" * 70)

        # 干员列表
        print(f"\n识别到 {result.total_operators} 个干员:")
        print("-" * 70)

        for i, card in enumerate(result.operators, 1):
            elite_str = f"E{card.elite_level.value}"
            row, col = card.grid_position

            print(f"\n{i}. 位置 [{row+1},{col+1}]: {card.name}")
            print(f"   精英化: {elite_str} | 等级: Lv.{card.level}")
            print(f"   识别置信度: {card.name_confidence:.2%}")

            if card.operator_info:
                info = card.operator_info
                stars = info.get('stars', 0)
                profession = info.get('profession', 'Unknown')
                sub_profession = info.get('sub_profession', 'Unknown')
                position = info.get('position', 'Unknown')
                cost = info.get('cost', '-')

                print(f"   星级: {'★' * stars if stars else 'Unknown'}")
                print(f"   职业: {profession} ({sub_profession})")
                print(f"   位置: {position}")
                print(f"   费用: {cost}")

                # 显示匹配信息
                match_type = info.get('match_type', 'none')
                match_score = info.get('match_score', 0.0)

                if match_type == 'fuzzy':
                    print(f"   ⚠️ 模糊匹配 (置信度: {match_score:.1%})")

                    # 显示备选结果
                    alternatives = info.get('alternatives', [])
                    if alternatives:
                        print(f"   备选:")
                        for alt in alternatives[:2]:
                            print(f"     - {alt['name']} ({alt['score']:.1%})")
                elif match_type == 'partial':
                    print(f"   ℹ️ 部分匹配 (置信度: {match_score:.1%})")

                # 如果OCR置信度低但匹配成功，显示提示
                if card.name_confidence < self._confidence_threshold and match_score > 0.8:
                    print(f"   ✓ 名称已校正: '{card.name}'")

            else:
                print("   (未找到数据库信息)")

        # 统计信息
        print("\n" + "=" * 70)
        print("统计信息")
        print("=" * 70)

        print(f"\n精英化分布:")
        for elite, count in sorted(result.elite_distribution.items()):
            elite_str = f"E{elite}" if elite > 0 else "E0"
            print(f"  {elite_str}: {count}人")

        print(f"\n平均等级: {result.average_level:.1f}")

        if result.profession_distribution:
            print(f"\n职业分布:")
            for prof, count in sorted(result.profession_distribution.items()):
                print(f"  {prof}: {count}人")

        if result.rarity_distribution:
            print(f"\n星级分布:")
            for rarity, count in sorted(result.rarity_distribution.items(), reverse=True):
                print(f"  {rarity}★: {count}人")

        print(f"\n估算总费用: {result.total_cost}")
        print("=" * 70)

    def visualize(
        self,
        image_path: Path,
        result: SquadAnalysisResult,
        output_path: Optional[Path] = None
    ) -> np.ndarray:
        """
        可视化分析结果

        Args:
            image_path: 原始图像路径
            result: 分析结果
            output_path: 输出路径（可选）

        Returns:
            可视化图像
        """
        image = cv2.imread(str(image_path))
        if image is None:
            logger.error(f"无法加载图像: {image_path}")
            return np.array([])

        # 使用识别器的可视化功能
        vis_image = self._recognizer.visualize_result(
            image,
            result.operators,
            output_path
        )

        # 添加额外信息
        h, w = vis_image.shape[:2]

        # 添加统计信息面板
        panel_height = 150
        panel = np.zeros((panel_height, w, 3), dtype=np.uint8)
        panel[:] = (40, 40, 40)

        # 绘制统计信息
        info_text = f"Total: {result.total_operators} | Avg Lv: {result.average_level:.1f} | Est Cost: {result.total_cost}"
        cv2.putText(panel, info_text, (10, 30),
                   cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)

        # 精英化分布
        elite_text = f"Elite: E0={result.elite_distribution.get(0,0)} E1={result.elite_distribution.get(1,0)} E2={result.elite_distribution.get(2,0)}"
        cv2.putText(panel, elite_text, (10, 60),
                   cv2.FONT_HERSHEY_SIMPLEX, 0.6, (200, 200, 200), 1)

        # 合并图像
        vis_image = np.vstack([vis_image, panel])

        # 保存
        if output_path:
            cv2.imwrite(str(output_path), vis_image)
            logger.info(f"可视化结果已保存: {output_path}")

        return vis_image

    def _sort_operators_for_deploy(self, cards: List[OperatorCard]) -> List[OperatorCard]:
        """
        按待部署区规则排序干员

        排序优先级：
        1. 初始部署费用升序
        2. 职业枚举值（近卫 > 狙击 > 重装 > 医疗 > 辅助 > 术师 > 特种 > 召唤物 > 装置 > 先锋）
        3. 单位ID文本升序（ASCII码比较）

        Args:
            cards: 干员卡片列表

        Returns:
            排序后的干员列表
        """
        # 职业优先级映射（数值越小优先级越高）
        profession_priority = {
            'WARRIOR': 1,    # 近卫
            'SNIPER': 2,     # 狙击
            'TANK': 3,       # 重装
            'MEDIC': 4,      # 医疗
            'SUPPORT': 5,    # 辅助
            'CASTER': 6,     # 术师
            'SPECIAL': 7,    # 特种
            'TOKEN': 8,      # 召唤物
            'TRAP': 9,       # 装置
            'PIONEER': 10,   # 先锋
        }

        def get_sort_key(card: OperatorCard) -> tuple:
            """生成排序键"""
            info = card.operator_info or {}

            # 1. 费用（升序，None或0放在最后）
            cost = info.get('cost', 0) or 999

            # 2. 职业优先级
            prof = info.get('profession', 'Unknown')
            prof_priority = profession_priority.get(prof, 99)

            # 3. 单位ID（ASCII升序）
            op_id = info.get('id', '')

            return (cost, prof_priority, op_id)

        return sorted(cards, key=get_sort_key)

    def get_deploy_list(self, result: SquadAnalysisResult) -> List[Dict[str, Any]]:
        """
        获取待部署区干员列表（已排序）

        Args:
            result: 分析结果

        Returns:
            待部署区干员列表，每个干员包含：
            - index: 序号
            - name: 干员名称
            - operator_id: 游戏内部ID
            - cost: 部署费用
            - profession: 职业
            - elite_level: 精英化等级
            - level: 等级
            - stars: 星级
        """
        deploy_list = []

        for idx, card in enumerate(result.sorted_operators, 1):
            info = card.operator_info or {}

            deploy_list.append({
                'index': idx,
                'name': card.name,
                'operator_id': info.get('id', ''),
                'cost': info.get('cost', 0),
                'profession': info.get('profession', 'Unknown'),
                'elite_level': card.elite_level.value,
                'level': card.level,
                'stars': info.get('stars', 0),
            })

        return deploy_list

    def print_deploy_list(self, result: SquadAnalysisResult) -> None:
        """
        打印待部署区干员列表

        Args:
            result: 分析结果
        """
        deploy_list = self.get_deploy_list(result)

        print("\n" + "=" * 80)
        print("待部署区干员列表（按费用→职业→ID排序）")
        print("=" * 80)
        print(f"{'序号':<6}{'干员名称':<12}{'费用':<8}{'职业':<12}{'精英':<8}{'等级':<8}{'ID':<20}")
        print("-" * 80)

        for op in deploy_list:
            elite_str = f"E{op['elite_level']}"
            print(f"{op['index']:<6}{op['name']:<12}{op['cost']:<8}{op['profession']:<12}{elite_str:<8}{op['level']:<8}{op['operator_id']:<20}")

        print("=" * 80)
        print(f"总计: {len(deploy_list)} 名干员")

    def shutdown(self) -> None:
        """关闭分析器"""
        if self._recognizer:
            self._recognizer.shutdown()
            self._recognizer = None

        if self._data_manager:
            self._data_manager.shutdown()
            self._data_manager = None

        self._initialized = False
        logger.info("编队分析器已关闭")
