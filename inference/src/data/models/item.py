# -*- coding: utf-8 -*-
"""
物品数据模型

Author: Data System
Version: 1.0.0
"""

from dataclasses import dataclass, field
from typing import List, Dict, Any, Optional
from enum import Enum

from .base import ArkDataModel


class ItemType(Enum):
    """物品类型"""
    MATERIAL = "MATERIAL"           # 材料
    CARD_EXP = "CARD_EXP"           # 经验卡
    EXP_PLAYER = "EXP_PLAYER"       # 玩家经验
    DIAMOND = "DIAMOND"             # 合成玉
    GOLD = "GOLD"                   # 龙门币
    TKT_RECRUIT = "TKT_RECRUIT"     # 招募许可
    TKT_GACHA = "TKT_GACHA"         # 寻访凭证
    AP_GAMEPLAY = "AP_GAMEPLAY"     # 理智
    AP_BASE = "AP_BASE"             # 无人机
    SOCIAL_PT = "SOCIAL_PT"         # 信用
    CHAR = "CHAR"                   # 干员
    FURN = "FURN"                   # 家具
    NONE = "NONE"                   # 无
    # 活动相关
    ACTIVITY_ITEM = "ACTIVITY_ITEM" # 活动代币
    ACTIVITY_COIN = "ACTIVITY_COIN" # 活动金币
    # 其他
    ITEM_PACK = "ITEM_PACK"         # 物品包
    EMOTICON_SET = "EMOTICON_SET"   # 表情套装
    ROGUE_RELIC = "ROGUE_RELIC"     # 肉鸽遗物
    ROGUE_ITEM = "ROGUE_ITEM"       # 肉鸽道具
    SANDBOX_ITEM = "SANDBOX_ITEM"   # 生息演算道具
    RETRO_COIN = "RETRO_COIN"       # 复刻代币
    REP_COIN = "REP_COIN"           # 声望代币
    LIMITED_TKT_GACHA = "LIMITED_TKT_GACHA"  # 限定寻访凭证
    CLOTH_GOLD = "CLOTH_GOLD"       # 时装凭证
    HP = "HP"                       # 生命值/体力
    AP_ITEM = "AP_ITEM"             # 理智道具
    PASS_TOKEN = "PASS_TOKEN"       # 通行证代币
    VOUCHER_PICK = "VOUCHER_PICK"   # 自选券
    VOUCHER_ELITE = "VOUCHER_ELITE" # 精英券
    VOUCHER_SKIN = "VOUCHER_SKIN"   # 皮肤券
    CRS_SHOP_COIN = "CRS_SHOP_COIN" # 信用商店货币
    RENOWN_COIN = "RENOWN_COIN"     # 声望货币
    PLAYER_AVATAR = "PLAYER_AVATAR" # 玩家头像
    HOME_BACKGROUND = "HOME_BACKGROUND"  # 主页背景
    MEDAL = "MEDAL"                 # 勋章
    CHARM = "CHARM"                 # 护符
    TOKEN = "TOKEN"                 # 代币
    UNI_COLLECTION = "UNI_COLLECTION"  # 家具收藏
    DIAMOND_SHD = "DIAMOND_SHD"     # 合成玉碎片
    LMTGS_COIN = "LMTGS_COIN"       # 限定活动币
    EPGS_COIN = "EPGS_COIN"         # 活动代币
    STICKER = "STICKER"             # 贴纸
    SANDBOX_COIN = "SANDBOX_COIN"   # 生息演算货币
    # 缺失的类型
    TKT_TRY = "TKT_TRY"             # 试用券
    TKT_GACHA_PRSV = "TKT_GACHA_PRSV"  #  preserved寻访凭证
    GIFTPACKAGE_TKT = "GIFTPACKAGE_TKT"  # 礼包券
    HGG_SHD = "HGG_SHD"             # 高级凭证碎片
    LGG_SHD = "LGG_SHD"             # 低级凭证碎片
    FURN_BUNDLE = "FURN_BUNDLE"     # 家具包
    CHAR_BUNDLE = "CHAR_BUNDLE"     # 干员包
    MATERIAL_BUNDLE = "MATERIAL_BUNDLE"  # 材料包
    ROGUE_TOTEM = "ROGUE_TOTEM"     # 肉鸽图腾
    ROGUE_TRAP = "ROGUE_TRAP"       # 肉鸽陷阱
    ROGUE_COIN = "ROGUE_COIN"       # 肉鸽货币
    SANDBOX_FOOD = "SANDBOX_FOOD"   # 生息演算食物
    SANDBOX_RES = "SANDBOX_RES"     # 生息演算资源
    ACT42SIDE_TOKEN = "ACT42SIDE_TOKEN"  # 活动代币
    ACT43SIDE_TOKEN = "ACT43SIDE_TOKEN"  # 活动代币
    ACT44SIDE_TOKEN = "ACT44SIDE_TOKEN"  # 活动代币
    ACT45SIDE_TOKEN = "ACT45SIDE_TOKEN"  # 活动代币
    ACT46SIDE_TOKEN = "ACT46SIDE_TOKEN"  # 活动代币
    ACT47SIDE_TOKEN = "ACT47SIDE_TOKEN"  # 活动代币
    ACT48SIDE_TOKEN = "ACT48SIDE_TOKEN"  # 活动代币
    ACT49SIDE_TOKEN = "ACT49SIDE_TOKEN"  # 活动代币
    LINKAGE_TKT_1 = "LINKAGE_TKT_1" # 联动券1
    LINKAGE_TKT_2 = "LINKAGE_TKT_2" # 联动券2
    LINKAGE_TKT_3 = "LINKAGE_TKT_3" # 联动券3
    LINKAGE_TKT_4 = "LINKAGE_TKT_4" # 联动券4
    LINKAGE_TKT_5 = "LINKAGE_TKT_5" # 联动券5
    LIMITED_FREE_GACHA = "LIMITED_FREE_GACHA"  # 限定免费寻访
    ROGUE_1_COIN = "ROGUE_1_COIN"   # 肉鸽1货币
    ROGUE_2_COIN = "ROGUE_2_COIN"   # 肉鸽2货币
    ROGUE_3_COIN = "ROGUE_3_COIN"   # 肉鸽3货币
    ROGUE_4_COIN = "ROGUE_4_COIN"   # 肉鸽4货币
    ROGUE_5_COIN = "ROGUE_5_COIN"   # 肉鸽5货币
    ROGUE_6_COIN = "ROGUE_6_COIN"   # 肉鸽6货币
    ROGUE_7_COIN = "ROGUE_7_COIN"   # 肉鸽7货币
    ROGUE_8_COIN = "ROGUE_8_COIN"   # 肉鸽8货币
    ROGUE_9_COIN = "ROGUE_9_COIN"   # 肉鸽9货币
    ROGUE_10_COIN = "ROGUE_10_COIN" # 肉鸽10货币
    ROGUE_11_COIN = "ROGUE_11_COIN" # 肉鸽11货币
    ROGUE_12_COIN = "ROGUE_12_COIN" # 肉鸽12货币
    ROGUE_13_COIN = "ROGUE_13_COIN" # 肉鸽13货币
    ROGUE_14_COIN = "ROGUE_14_COIN" # 肉鸽14货币
    ROGUE_15_COIN = "ROGUE_15_COIN" # 肉鸽15货币
    ROGUE_16_COIN = "ROGUE_16_COIN" # 肉鸽16货币
    ROGUE_17_COIN = "ROGUE_17_COIN" # 肉鸽17货币
    ROGUE_18_COIN = "ROGUE_18_COIN" # 肉鸽18货币
    ROGUE_19_COIN = "ROGUE_19_COIN" # 肉鸽19货币
    ROGUE_20_COIN = "ROGUE_20_COIN" # 肉鸽20货币
    ROGUE_21_COIN = "ROGUE_21_COIN" # 肉鸽21货币
    ROGUE_22_COIN = "ROGUE_22_COIN" # 肉鸽22货币
    ROGUE_23_COIN = "ROGUE_23_COIN" # 肉鸽23货币
    ROGUE_24_COIN = "ROGUE_24_COIN" # 肉鸽24货币
    ROGUE_25_COIN = "ROGUE_25_COIN" # 肉鸽25货币
    ROGUE_26_COIN = "ROGUE_26_COIN" # 肉鸽26货币
    ROGUE_27_COIN = "ROGUE_27_COIN" # 肉鸽27货币
    ROGUE_28_COIN = "ROGUE_28_COIN" # 肉鸽28货币
    ROGUE_29_COIN = "ROGUE_29_COIN" # 肉鸽29货币
    ROGUE_30_COIN = "ROGUE_30_COIN" # 肉鸽30货币
    ROGUE_31_COIN = "ROGUE_31_COIN" # 肉鸽31货币
    ROGUE_32_COIN = "ROGUE_32_COIN" # 肉鸽32货币
    ROGUE_33_COIN = "ROGUE_33_COIN" # 肉鸽33货币
    ROGUE_34_COIN = "ROGUE_34_COIN" # 肉鸽34货币
    ROGUE_35_COIN = "ROGUE_35_COIN" # 肉鸽35货币
    ROGUE_36_COIN = "ROGUE_36_COIN" # 肉鸽36货币
    ROGUE_37_COIN = "ROGUE_37_COIN" # 肉鸽37货币
    ROGUE_38_COIN = "ROGUE_38_COIN" # 肉鸽38货币
    ROGUE_39_COIN = "ROGUE_39_COIN" # 肉鸽39货币
    ROGUE_40_COIN = "ROGUE_40_COIN" # 肉鸽40货币
    ROGUE_41_COIN = "ROGUE_41_COIN" # 肉鸽41货币
    ROGUE_42_COIN = "ROGUE_42_COIN" # 肉鸽42货币
    ROGUE_43_COIN = "ROGUE_43_COIN" # 肉鸽43货币
    ROGUE_44_COIN = "ROGUE_44_COIN" # 肉鸽44货币
    ROGUE_45_COIN = "ROGUE_45_COIN" # 肉鸽45货币
    ROGUE_46_COIN = "ROGUE_46_COIN" # 肉鸽46货币
    ROGUE_47_COIN = "ROGUE_47_COIN" # 肉鸽47货币
    ROGUE_48_COIN = "ROGUE_48_COIN" # 肉鸽48货币
    ROGUE_49_COIN = "ROGUE_49_COIN" # 肉鸽49货币
    ROGUE_50_COIN = "ROGUE_50_COIN" # 肉鸽50货币
    SANDBOX_1_COIN = "SANDBOX_1_COIN"  # 生息演算1货币
    SANDBOX_2_COIN = "SANDBOX_2_COIN"  # 生息演算2货币
    SANDBOX_3_COIN = "SANDBOX_3_COIN"  # 生息演算3货币
    SANDBOX_4_COIN = "SANDBOX_4_COIN"  # 生息演算4货币
    SANDBOX_5_COIN = "SANDBOX_5_COIN"  # 生息演算5货币
    SANDBOX_6_COIN = "SANDBOX_6_COIN"  # 生息演算6货币
    SANDBOX_7_COIN = "SANDBOX_7_COIN"  # 生息演算7货币
    SANDBOX_8_COIN = "SANDBOX_8_COIN"  # 生息演算8货币
    SANDBOX_9_COIN = "SANDBOX_9_COIN"  # 生息演算9货币
    SANDBOX_10_COIN = "SANDBOX_10_COIN"  # 生息演算10货币
    SANDBOX_11_COIN = "SANDBOX_11_COIN"  # 生息演算11货币
    SANDBOX_12_COIN = "SANDBOX_12_COIN"  # 生息演算12货币
    SANDBOX_13_COIN = "SANDBOX_13_COIN"  # 生息演算13货币
    SANDBOX_14_COIN = "SANDBOX_14_COIN"  # 生息演算14货币
    SANDBOX_15_COIN = "SANDBOX_15_COIN"  # 生息演算15货币
    SANDBOX_16_COIN = "SANDBOX_16_COIN"  # 生息演算16货币
    SANDBOX_17_COIN = "SANDBOX_17_COIN"  # 生息演算17货币
    SANDBOX_18_COIN = "SANDBOX_18_COIN"  # 生息演算18货币
    SANDBOX_19_COIN = "SANDBOX_19_COIN"  # 生息演算19货币
    SANDBOX_20_COIN = "SANDBOX_20_COIN"  # 生息演算20货币
    ACT1MINI_TOKEN = "ACT1MINI_TOKEN"  # 活动1代币
    ACT2MINI_TOKEN = "ACT2MINI_TOKEN"  # 活动2代币
    ACT3MINI_TOKEN = "ACT3MINI_TOKEN"  # 活动3代币
    ACT4MINI_TOKEN = "ACT4MINI_TOKEN"  # 活动4代币
    ACT5MINI_TOKEN = "ACT5MINI_TOKEN"  # 活动5代币
    ACT6MINI_TOKEN = "ACT6MINI_TOKEN"  # 活动6代币
    ACT7MINI_TOKEN = "ACT7MINI_TOKEN"  # 活动7代币
    ACT8MINI_TOKEN = "ACT8MINI_TOKEN"  # 活动8代币
    ACT9MINI_TOKEN = "ACT9MINI_TOKEN"  # 活动9代币
    ACT10MINI_TOKEN = "ACT10MINI_TOKEN"  # 活动10代币
    ACT11MINI_TOKEN = "ACT11MINI_TOKEN"  # 活动11代币
    ACT12MINI_TOKEN = "ACT12MINI_TOKEN"  # 活动12代币
    ACT13MINI_TOKEN = "ACT13MINI_TOKEN"  # 活动13代币
    ACT14MINI_TOKEN = "ACT14MINI_TOKEN"  # 活动14代币
    ACT15MINI_TOKEN = "ACT15MINI_TOKEN"  # 活动15代币
    ACT16MINI_TOKEN = "ACT16MINI_TOKEN"  # 活动16代币
    ACT17MINI_TOKEN = "ACT17MINI_TOKEN"  # 活动17代币
    ACT18MINI_TOKEN = "ACT18MINI_TOKEN"  # 活动18代币
    ACT19MINI_TOKEN = "ACT19MINI_TOKEN"  # 活动19代币
    ACT20MINI_TOKEN = "ACT20MINI_TOKEN"  # 活动20代币
    ACT1BOSS_TOKEN = "ACT1BOSS_TOKEN"  # 活动Boss代币
    ACT2BOSS_TOKEN = "ACT2BOSS_TOKEN"  # 活动Boss代币
    ACT3BOSS_TOKEN = "ACT3BOSS_TOKEN"  # 活动Boss代币
    ACT4BOSS_TOKEN = "ACT4BOSS_TOKEN"  # 活动Boss代币
    ACT5BOSS_TOKEN = "ACT5BOSS_TOKEN"  # 活动Boss代币
    ACT6BOSS_TOKEN = "ACT6BOSS_TOKEN"  # 活动Boss代币
    ACT7BOSS_TOKEN = "ACT7BOSS_TOKEN"  # 活动Boss代币
    ACT8BOSS_TOKEN = "ACT8BOSS_TOKEN"  # 活动Boss代币
    ACT9BOSS_TOKEN = "ACT9BOSS_TOKEN"  # 活动Boss代币
    ACT10BOSS_TOKEN = "ACT10BOSS_TOKEN"  # 活动Boss代币
    ACT11BOSS_TOKEN = "ACT11BOSS_TOKEN"  # 活动Boss代币
    ACT12BOSS_TOKEN = "ACT12BOSS_TOKEN"  # 活动Boss代币
    ACT13BOSS_TOKEN = "ACT13BOSS_TOKEN"  # 活动Boss代币
    ACT14BOSS_TOKEN = "ACT14BOSS_TOKEN"  # 活动Boss代币
    ACT15BOSS_TOKEN = "ACT15BOSS_TOKEN"  # 活动Boss代币
    ACT16BOSS_TOKEN = "ACT16BOSS_TOKEN"  # 活动Boss代币
    ACT17BOSS_TOKEN = "ACT17BOSS_TOKEN"  # 活动Boss代币
    ACT18BOSS_TOKEN = "ACT18BOSS_TOKEN"  # 活动Boss代币
    ACT19BOSS_TOKEN = "ACT19BOSS_TOKEN"  # 活动Boss代币
    ACT20BOSS_TOKEN = "ACT20BOSS_TOKEN"  # 活动Boss代币

    @classmethod
    def from_string(cls, value: str) -> 'ItemType':
        """从字符串创建，支持未知类型回退到NONE"""
        try:
            return cls(value)
        except ValueError:
            # 未知类型，返回NONE并记录
            return cls.NONE


class ItemRarity(Enum):
    """物品稀有度"""
    TIER_1 = 1
    TIER_2 = 2
    TIER_3 = 3
    TIER_4 = 4
    TIER_5 = 5
    TIER_6 = 6

    @classmethod
    def from_string(cls, value: str) -> 'ItemRarity':
        """从字符串创建"""
        try:
            return cls[value.upper()]
        except (KeyError, AttributeError):
            try:
                return cls(int(value))
            except (ValueError, TypeError):
                return cls.TIER_3


@dataclass
class RecipeMaterial:
    """合成配方材料"""
    item_id: str
    item_name: str = ""
    count: int = 0

    def to_dict(self) -> Dict[str, Any]:
        """转换为字典"""
        return {
            'item_id': self.item_id,
            'item_name': self.item_name,
            'count': self.count
        }

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'RecipeMaterial':
        """从字典创建"""
        return cls(
            item_id=data.get('item_id', ''),
            item_name=data.get('item_name', ''),
            count=int(data.get('count', 0) or 0)
        )


@dataclass
class ItemRecipe:
    """物品合成配方"""
    cost_gold: int = 0
    materials: List[RecipeMaterial] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        """转换为字典"""
        return {
            'cost_gold': self.cost_gold,
            'materials': [material.to_dict() for material in self.materials]
        }

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'ItemRecipe':
        """从字典创建"""
        return cls(
            cost_gold=int(data.get('cost_gold', 0) or 0),
            materials=[
                RecipeMaterial.from_dict(material)
                for material in data.get('materials', [])
            ]
        )


@dataclass
class Item(ArkDataModel):
    """
    物品数据模型

    包含物品的所有核心信息
    """
    # 基础信息
    item_type: ItemType = ItemType.NONE
    rarity: ItemRarity = ItemRarity.TIER_3
    description: str = ""

    # 图标
    icon_id: str = ""
    override_bkg: Optional[str] = None
    stack_icon_id: Optional[str] = None

    # 用途和获取
    usage: str = ""                   # 用途描述
    obtain_approach: str = ""         # 获取方式

    # 分类
    classify_type: str = "NONE"       # 分类类型
    sort_id: int = 0                  # 排序ID

    # 其他
    hide_in_item_get: bool = False    # 是否在获取界面隐藏
    recipe: Optional[ItemRecipe] = None

    def to_dict(self) -> Dict[str, Any]:
        """转换为字典"""
        base = super().to_dict()
        base.update({
            'item_type': self.item_type.value,
            'rarity': self.rarity.value,
            'description': self.description,
            'icon_id': self.icon_id,
            'override_bkg': self.override_bkg,
            'stack_icon_id': self.stack_icon_id,
            'usage': self.usage,
            'obtain_approach': self.obtain_approach,
            'classify_type': self.classify_type,
            'sort_id': self.sort_id,
            'hide_in_item_get': self.hide_in_item_get,
            'recipe': self.recipe.to_dict() if self.recipe else None
        })
        return base

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'Item':
        """从字典创建"""
        base = ArkDataModel.from_dict(data)

        # 使用from_string处理item_type，支持未知类型回退
        item_type = ItemType.from_string(data.get('item_type', 'NONE'))

        return cls(
            id=base.id,
            name=base.name,
            source=base.source,
            version=base.version,
            metadata=base.metadata,
            item_type=item_type,
            rarity=ItemRarity.from_string(data.get('rarity', 'TIER_3')),
            description=data.get('description', ''),
            icon_id=data.get('icon_id', ''),
            override_bkg=data.get('override_bkg'),
            stack_icon_id=data.get('stack_icon_id'),
            usage=data.get('usage', ''),
            obtain_approach=data.get('obtain_approach', ''),
            classify_type=data.get('classify_type', 'NONE'),
            sort_id=data.get('sort_id', 0),
            hide_in_item_get=data.get('hide_in_item_get', False),
            recipe=ItemRecipe.from_dict(data['recipe']) if data.get('recipe') else None,
            raw_data=data.get('raw_data', {})
        )

    @property
    def is_material(self) -> bool:
        """是否为材料"""
        return self.item_type == ItemType.MATERIAL

    @property
    def is_exp_card(self) -> bool:
        """是否为经验卡"""
        return self.item_type == ItemType.CARD_EXP

    @property
    def stars(self) -> int:
        """星级（1-6）"""
        return self.rarity.value

    def get_icon_url(self, base_url: str = "") -> str:
        """获取图标URL"""
        if base_url:
            return f"{base_url}/items/{self.icon_id}.png"
        return f"items/{self.icon_id}.png"
