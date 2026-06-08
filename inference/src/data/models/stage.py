# -*- coding: utf-8 -*-
"""
关卡数据模型

Author: Data System
Version: 1.0.0
"""

from dataclasses import dataclass, field
from typing import List, Dict, Any, Optional
from enum import Enum

from .base import ArkDataModel


class StageType(Enum):
    """关卡类型"""
    MAIN = "MAIN"           # 主线
    SUB = "SUB"             # 支线
    DAILY = "DAILY"         # 日常
    ACTIVITY = "ACTIVITY"   # 活动
    GUIDE = "GUIDE"         # 教学
    CAMPAIGN = "CAMPAIGN"   # 剿灭
    ROGUELIKE = "ROGUELIKE" # 肉鸽
    CLIMB_TOWER = "CLIMB_TOWER" # 保全派驻/爬塔类
    SPECIAL_STORY = "SPECIAL_STORY" # 特别剧情


class Difficulty(Enum):
    """难度级别"""
    NORMAL = "NORMAL"
    HARD = "HARD"
    FOUR_STAR = "FOUR_STAR"
    SIX_STAR = "SIX_STAR"


@dataclass
class StageDrop:
    """关卡掉落物品"""
    item_id: str
    item_name: str
    drop_type: str          # ONCE, NORMAL, COMPLETE, ADDITIONAL
    occ_percent: str        # ALWAYS, SOMETIMES, ALMOST
    count: int = 1

    def to_dict(self) -> Dict[str, Any]:
        return {
            'item_id': self.item_id,
            'item_name': self.item_name,
            'drop_type': self.drop_type,
            'occ_percent': self.occ_percent,
            'count': self.count
        }

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'StageDrop':
        return cls(
            item_id=data.get('item_id', ''),
            item_name=data.get('item_name', ''),
            drop_type=data.get('drop_type', 'NORMAL'),
            occ_percent=data.get('occ_percent', 'SOMETIMES'),
            count=data.get('count', 1)
        )


@dataclass
class StageCondition:
    """关卡解锁条件"""
    condition_type: str     # STAGE, LEVEL, etc.
    value: str

    def to_dict(self) -> Dict[str, Any]:
        return {
            'condition_type': self.condition_type,
            'value': self.value
        }


@dataclass
class Stage(ArkDataModel):
    """
    关卡数据模型

    包含关卡的所有核心信息：消耗、掉落、解锁条件等
    """
    # 基础信息
    stage_type: StageType = StageType.MAIN
    difficulty: Difficulty = Difficulty.NORMAL
    code: str = ""                    # 关卡代码，如 "1-7"
    description: str = ""

    # 区域信息
    zone_id: str = ""
    level_id: str = ""                # 关卡文件ID

    # 消耗与奖励
    ap_cost: int = 0                  # 理智消耗
    ap_fail_return: int = 0           # 失败返还理智
    exp_gain: int = 0                 # 经验获得
    gold_gain: int = 0                # 龙门币获得

    # 特殊标记
    can_practice: bool = True         # 是否可以演习
    can_battle_replay: bool = True    # 是否可以代理
    is_story_only: bool = False       # 是否仅剧情
    boss_mark: bool = False           # 是否有Boss

    # 掉落信息
    drops: List[StageDrop] = field(default_factory=list)

    # 解锁条件
    unlock_conditions: List[StageCondition] = field(default_factory=list)

    # 其他
    danger_level: str = ""            # 推荐等级
    loading_pic_id: str = ""
    max_slot: int = -1                # 最大编队数

    def to_dict(self) -> Dict[str, Any]:
        """转换为字典"""
        base = super().to_dict()
        base.update({
            'stage_type': self.stage_type.value,
            'difficulty': self.difficulty.value,
            'code': self.code,
            'description': self.description,
            'zone_id': self.zone_id,
            'level_id': self.level_id,
            'ap_cost': self.ap_cost,
            'ap_fail_return': self.ap_fail_return,
            'exp_gain': self.exp_gain,
            'gold_gain': self.gold_gain,
            'can_practice': self.can_practice,
            'can_battle_replay': self.can_battle_replay,
            'is_story_only': self.is_story_only,
            'boss_mark': self.boss_mark,
            'drops': [d.to_dict() for d in self.drops],
            'unlock_conditions': [c.to_dict() for c in self.unlock_conditions],
            'danger_level': self.danger_level,
            'loading_pic_id': self.loading_pic_id,
            'max_slot': self.max_slot
        })
        return base

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'Stage':
        """从字典创建"""
        base = ArkDataModel.from_dict(data)

        return cls(
            id=base.id,
            name=base.name,
            source=base.source,
            version=base.version,
            metadata=base.metadata,
            stage_type=StageType(data.get('stage_type', 'MAIN')),
            difficulty=Difficulty(data.get('difficulty', 'NORMAL')),
            code=data.get('code', ''),
            description=data.get('description', ''),
            zone_id=data.get('zone_id', ''),
            level_id=data.get('level_id', ''),
            ap_cost=data.get('ap_cost', 0),
            ap_fail_return=data.get('ap_fail_return', 0),
            exp_gain=data.get('exp_gain', 0),
            gold_gain=data.get('gold_gain', 0),
            can_practice=data.get('can_practice', True),
            can_battle_replay=data.get('can_battle_replay', True),
            is_story_only=data.get('is_story_only', False),
            boss_mark=data.get('boss_mark', False),
            drops=[StageDrop.from_dict(d) for d in data.get('drops', [])],
            unlock_conditions=[StageCondition(**c) for c in data.get('unlock_conditions', [])],
            danger_level=data.get('danger_level', ''),
            loading_pic_id=data.get('loading_pic_id', ''),
            max_slot=data.get('max_slot', -1),
            raw_data=data.get('raw_data', {})
        )

    @property
    def is_main_stage(self) -> bool:
        """是否为主线关卡"""
        return self.stage_type == StageType.MAIN

    @property
    def is_resource_stage(self) -> bool:
        """是否为资源关卡"""
        return self.stage_type == StageType.DAILY

    @property
    def is_campaign(self) -> bool:
        """是否为剿灭作战"""
        return self.stage_type == StageType.CAMPAIGN

    def get_main_drops(self) -> List[StageDrop]:
        """获取主要掉落物品"""
        return [d for d in self.drops if d.drop_type in ['NORMAL', 'ONCE']]

    def get_additional_drops(self) -> List[StageDrop]:
        """获取额外掉落物品"""
        return [d for d in self.drops if d.drop_type == 'ADDITIONAL']
