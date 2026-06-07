# -*- coding: utf-8 -*-
"""
GitHub数据提供者

从ArknightsGameData仓库同步游戏数据

Author: Data System
Version: 1.0.0
"""

import json
import logging
import subprocess
import shutil
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Any, Callable
from dataclasses import dataclass
import threading
import time

from ..models.base import DataSource, DataVersion
from ..models.operator import Operator, OperatorPhase, OperatorSkill, OperatorTalent
from ..models.operator import OperatorProfession, OperatorRarity, PositionType
from ..models.stage import Stage, StageDrop, StageType, Difficulty
from ..models.item import Item, ItemType, ItemRarity, ItemRecipe, RecipeMaterial
from ..models.enemy import Enemy, EnemyAbility, EnemyLevel

logger = logging.getLogger(__name__)


@dataclass
class SyncConfig:
    """同步配置"""
    repo_url: str = "https://github.com/Kengxxiao/ArknightsGameData.git"
    local_path: Path = Path("ArknightsGameData")
    data_root: str = "zh_CN"
    data_version_file: str = "data_version.txt"
    auto_pull: bool = True
    pull_interval_hours: int = 24
    sparse_paths: Optional[List[str]] = None


class GitHubDataProvider:
    """
    GitHub数据提供者

    负责从ArknightsGameData GitHub仓库同步和解析游戏数据
    """

    # 数据文件映射
    DATA_FILES = {
        'character': 'gamedata/excel/character_table.json',
        'stage': 'gamedata/excel/stage_table.json',
        'item': 'gamedata/excel/item_table.json',
        'building': 'gamedata/excel/building_data.json',
        'enemy': 'gamedata/excel/enemy_handbook_table.json',
        'skill': 'gamedata/excel/skill_table.json',
        'zone': 'gamedata/excel/zone_table.json',
    }

    def __init__(self, config: Optional[SyncConfig] = None):
        """
        初始化GitHub数据提供者

        Args:
            config: 同步配置，使用默认配置如果为None
        """
        self.config = config or SyncConfig()
        self._data_cache: Dict[str, Any] = {}
        self._last_sync: Optional[datetime] = None
        self._version: Optional[DataVersion] = None
        self._lock = threading.RLock()
        self._initialized = False

    def initialize(self) -> bool:
        """
        初始化数据提供者

        Returns:
            是否初始化成功
        """
        try:
            logger.info("初始化GitHub数据提供者...")

            # 检查本地仓库是否存在
            if not self.config.local_path.exists():
                logger.info(f"本地仓库不存在，准备克隆: {self.config.repo_url}")
                if not self._clone_repo():
                    return False
            elif self.config.auto_pull:
                logger.info("本地仓库存在，准备更新")
                if not self._pull_repo():
                    return False

            # 加载数据版本
            self._load_version()

            self._initialized = True
            logger.info("GitHub数据提供者初始化完成")
            return True

        except Exception as e:
            logger.error(f"初始化失败: {e}")
            return False

    def _clone_repo(self) -> bool:
        """
        克隆GitHub仓库

        Returns:
            是否克隆成功
        """
        try:
            cmd = [
                'git', 'clone',
                '--depth', '1',
            ]
            if self.config.sparse_paths:
                cmd.extend(['--filter=blob:none', '--sparse'])
            cmd.extend([self.config.repo_url, str(self.config.local_path)])

            logger.info(f"执行命令: {' '.join(cmd)}")
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                encoding='utf-8',
                timeout=300
            )

            if result.returncode == 0:
                if self.config.sparse_paths and not self._apply_sparse_checkout():
                    return False
                logger.info("仓库克隆成功")
                return True
            else:
                logger.error(f"克隆失败: {result.stderr}")
                return False

        except subprocess.TimeoutExpired:
            logger.error("克隆超时")
            return False
        except Exception as e:
            logger.error(f"克隆异常: {e}")
            return False

    def _pull_repo(self) -> bool:
        """
        拉取最新数据

        Returns:
            是否拉取成功
        """
        try:
            cmd = ['git', '-C', str(self.config.local_path), 'pull']

            logger.info(f"执行命令: {' '.join(cmd)}")
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                encoding='utf-8',
                timeout=60
            )

            if result.returncode == 0:
                if self.config.sparse_paths and not self._apply_sparse_checkout():
                    return False
                if 'Already up to date' in result.stdout:
                    logger.info("数据已是最新")
                else:
                    logger.info("数据更新成功")
                return True
            else:
                logger.error(f"拉取失败: {result.stderr}")
                return False

        except subprocess.TimeoutExpired:
            logger.error("拉取超时")
            return False
        except Exception as e:
            logger.error(f"拉取异常: {e}")
            return False

    def _apply_sparse_checkout(self) -> bool:
        """应用 sparse checkout 路径"""
        if not self.config.sparse_paths:
            return True

        try:
            cmd = [
                'git', '-C', str(self.config.local_path),
                'sparse-checkout', 'set',
                *self.config.sparse_paths
            ]
            logger.info(f"执行命令: {' '.join(cmd)}")
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                encoding='utf-8',
                timeout=60
            )

            if result.returncode == 0:
                return True

            logger.error(f"sparse checkout 失败: {result.stderr}")
            return False

        except subprocess.TimeoutExpired:
            logger.error("sparse checkout 超时")
            return False
        except Exception as e:
            logger.error(f"sparse checkout 异常: {e}")
            return False

    def _load_version(self) -> None:
        """加载数据版本信息"""
        try:
            version_file = self.config.local_path / self.config.data_root / 'gamedata/excel/data_version.txt'
            if version_file.exists():
                with open(version_file, 'r', encoding='utf-8') as f:
                    version_str = f.read().strip()

                self._version = DataVersion(
                    version=version_str,
                    source=DataSource.GITHUB,
                    updated_at=datetime.now(),
                    description="ArknightsGameData GitHub仓库"
                )
                logger.info(f"数据版本: {version_str}")
            else:
                logger.warning("未找到版本文件")
                self._version = DataVersion(
                    version="unknown",
                    source=DataSource.GITHUB,
                    updated_at=datetime.now()
                )

        except Exception as e:
            logger.error(f"加载版本失败: {e}")
            self._version = DataVersion(
                version="unknown",
                source=DataSource.GITHUB,
                updated_at=datetime.now()
            )

    def sync(self, force: bool = False) -> bool:
        """
        同步数据

        Args:
            force: 是否强制同步（忽略时间间隔）

        Returns:
            是否同步成功
        """
        with self._lock:
            if not self._initialized:
                logger.error("提供者未初始化")
                return False

            # 检查是否需要同步
            if not force and self._last_sync:
                elapsed = (datetime.now() - self._last_sync).total_seconds()
                if elapsed < self.config.pull_interval_hours * 3600:
                    logger.debug(f"距离上次同步仅 {elapsed/3600:.1f} 小时，跳过")
                    return True

            logger.info("开始同步数据...")

            if not self._pull_repo():
                return False

            self._load_version()
            self._last_sync = datetime.now()

            # 清空缓存，强制重新加载
            self._data_cache.clear()

            logger.info("数据同步完成")
            return True

    def _load_json(self, data_type: str) -> Optional[Dict[str, Any]]:
        """
        加载JSON数据文件

        Args:
            data_type: 数据类型

        Returns:
            JSON数据字典或None
        """
        with self._lock:
            # 检查缓存
            if data_type in self._data_cache:
                return self._data_cache[data_type]

            # 加载文件
            relative_path = self.DATA_FILES.get(data_type, '')
            file_path = self.config.local_path / self.config.data_root / relative_path
            if not file_path.exists():
                logger.error(f"数据文件不存在: {file_path}")
                return None

            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    data = json.load(f)

                self._data_cache[data_type] = data
                logger.debug(f"加载数据文件: {file_path}")
                return data

            except json.JSONDecodeError as e:
                logger.error(f"JSON解析失败: {e}")
                return None
            except Exception as e:
                logger.error(f"加载数据失败: {e}")
                return None

    def get_operators(self, progress_callback: Optional[Callable[[int, int], None]] = None) -> List[Operator]:
        """
        获取所有干员数据

        Args:
            progress_callback: 进度回调函数(current, total)

        Returns:
            干员列表
        """
        data = self._load_json('character')
        if not data:
            return []

        operators = []
        char_data = data.get('chars', data)  # 兼容不同格式

        if isinstance(char_data, dict):
            items = list(char_data.items())
        else:
            items = []

        total = len(items)

        for idx, (char_id, char_info) in enumerate(items):
            try:
                if progress_callback:
                    progress_callback(idx + 1, total)

                operator = self._parse_operator(char_id, char_info)
                if operator:
                    operators.append(operator)

            except Exception as e:
                logger.warning(f"解析干员 {char_id} 失败: {e}")
                continue

        logger.info(f"加载了 {len(operators)} 个干员")
        return operators

    def _parse_operator(self, char_id: str, data: Dict[str, Any]) -> Optional[Operator]:
        """解析干员数据"""
        try:
            # 解析精英化阶段
            phases = []
            phases_data = data.get('phases') or []  # 处理None情况
            for phase_index, phase_data in enumerate(phases_data):
                if phase_data and phase_data.get('attributesKeyFrames'):
                    # 使用数组索引作为精英化阶段 (0=E0, 1=E1, 2=E2)
                    max_frame = phase_data['attributesKeyFrames'][-1]
                    attr = max_frame.get('data', {})

                    phase = OperatorPhase(
                        phase_index=phase_index,  # 数组索引即为精英化阶段
                        max_level=phase_data.get('maxLevel', 1),
                        max_hp=int(attr.get('maxHp', 0)),
                        atk=int(attr.get('atk', 0)),
                        def_=int(attr.get('def', 0)),
                        magic_resistance=float(attr.get('magicResistance', 0)),
                        cost=int(attr.get('cost', 0)),
                        block_count=int(attr.get('blockCnt', 1)),
                        attack_speed=float(attr.get('attackSpeed', 100)),
                        respawn_time=int(attr.get('respawnTime', 70))
                    )
                    phases.append(phase)

            # 解析技能
            skills = []
            skills_data = data.get('skills') or []  # 处理None情况
            for skill_data in skills_data:
                if not skill_data:
                    continue
                skill_id = skill_data.get('skillId', '')
                if skill_id:
                    skill = OperatorSkill(
                        skill_id=skill_id,
                        skill_name=skill_id,  # 需要从skill_table获取名称
                        description="",
                        sp_cost=0,
                        sp_initial=0,
                        duration=0.0
                    )
                    skills.append(skill)

            # 解析天赋
            talents = []
            talents_data = data.get('talents') or []  # 处理None情况
            for talent_data in talents_data:
                if not talent_data:
                    continue
                for candidate in talent_data.get('candidates') or []:
                    if not candidate:
                        continue
                    talent = OperatorTalent(
                        talent_id=candidate.get('unlockCondition', {}).get('phase', '0'),
                        talent_name=candidate.get('name', ''),
                        description=candidate.get('description', ''),
                        unlock_phase=candidate.get('unlockCondition', {}).get('phase', 0),
                        unlock_level=candidate.get('unlockCondition', {}).get('level', 1)
                    )
                    talents.append(talent)

            return Operator(
                id=char_id,
                name=data.get('name', char_id),
                source=DataSource.GITHUB,
                version=self._version,
                appellation=data.get('appellation', ''),
                profession=OperatorProfession.from_string(data.get('profession', 'WARRIOR')),
                sub_profession_id=data.get('subProfessionId', ''),
                rarity=OperatorRarity.from_string(data.get('rarity', 'TIER_3')),
                position=PositionType(data.get('position', 'MELEE')),
                description=data.get('description', ''),
                item_usage=data.get('itemUsage', ''),
                item_desc=data.get('itemDesc', ''),
                obtain_approach=data.get('itemObtainApproach', ''),
                tag_list=data.get('tagList', []),
                phases=phases,
                skills=skills,
                talents=talents,
                max_potential_level=data.get('maxPotentialLevel', 5),
                potential_item_id=data.get('potentialItemId', ''),
                nation_id=data.get('nationId', ''),
                group_id=data.get('groupId'),
                team_id=data.get('teamId'),
                display_number=data.get('displayNumber', ''),
                is_not_obtainable=data.get('isNotObtainable', False),
                is_sp_char=data.get('isSpChar', False),
                raw_data=data
            )

        except Exception as e:
            logger.error(f"解析干员 {char_id} 数据失败: {e}")
            return None

    def get_stages(self, progress_callback: Optional[Callable[[int, int], None]] = None) -> List[Stage]:
        """
        获取所有关卡数据

        Args:
            progress_callback: 进度回调函数

        Returns:
            关卡列表
        """
        data = self._load_json('stage')
        if not data:
            return []

        stages = []
        stages_data = data.get('stages', {})

        items = list(stages_data.items())
        total = len(items)

        for idx, (stage_id, stage_info) in enumerate(items):
            try:
                if progress_callback:
                    progress_callback(idx + 1, total)

                stage = self._parse_stage(stage_id, stage_info)
                if stage:
                    stages.append(stage)

            except Exception as e:
                logger.warning(f"解析关卡 {stage_id} 失败: {e}")
                continue

        logger.info(f"加载了 {len(stages)} 个关卡")
        return stages

    def _parse_stage(self, stage_id: str, data: Dict[str, Any]) -> Optional[Stage]:
        """解析关卡数据"""
        try:
            # 解析掉落
            drops = []
            drop_info = data.get('stageDropInfo', {})
            for reward in drop_info.get('displayDetailRewards', []):
                drop = StageDrop(
                    item_id=reward.get('id', ''),
                    item_name=reward.get('type', ''),  # 需要映射
                    drop_type=reward.get('dropType', 'NORMAL'),
                    occ_percent=reward.get('occPercent', 'SOMETIMES')
                )
                drops.append(drop)

            return Stage(
                id=stage_id,
                name=data.get('name', stage_id),
                source=DataSource.GITHUB,
                version=self._version,
                stage_type=StageType(data.get('stageType', 'MAIN')),
                difficulty=Difficulty(data.get('difficulty', 'NORMAL')),
                code=data.get('code', ''),
                description=data.get('description', ''),
                zone_id=data.get('zoneId', ''),
                level_id=data.get('levelId', ''),
                ap_cost=data.get('apCost', 0),
                ap_fail_return=data.get('apFailReturn', 0),
                exp_gain=data.get('expGain', 0),
                gold_gain=data.get('goldGain', 0),
                can_practice=data.get('canPractice', True),
                can_battle_replay=data.get('canBattleReplay', True),
                is_story_only=data.get('isStoryOnly', False),
                boss_mark=data.get('bossMark', False),
                drops=drops,
                danger_level=data.get('dangerLevel', ''),
                loading_pic_id=data.get('loadingPicId', ''),
                max_slot=data.get('maxSlot', -1),
                raw_data=data
            )

        except Exception as e:
            logger.error(f"解析关卡 {stage_id} 数据失败: {e}")
            return None

    def get_items(self, progress_callback: Optional[Callable[[int, int], None]] = None) -> List[Item]:
        """
        获取所有物品数据

        Args:
            progress_callback: 进度回调函数

        Returns:
            物品列表
        """
        data = self._load_json('item')
        if not data:
            return []

        items = []
        items_data = data.get('items', {})
        recipes = self._build_item_recipes(items_data)

        item_list = list(items_data.items())
        total = len(item_list)

        for idx, (item_id, item_info) in enumerate(item_list):
            try:
                if progress_callback:
                    progress_callback(idx + 1, total)

                item = self._parse_item(item_id, item_info, recipes.get(item_id))
                if item:
                    items.append(item)

            except Exception as e:
                logger.warning(f"解析物品 {item_id} 失败: {e}")
                continue

        logger.info(f"加载了 {len(items)} 个物品")
        return items

    def _build_item_recipes(self, items_data: Dict[str, Any]) -> Dict[str, ItemRecipe]:
        """从基建配方表构建物品合成配方索引"""
        building_data = self._load_json('building')
        if not building_data:
            return {}

        formula_tables = {
            'WORKSHOP': building_data.get('workshopFormulas', {}),
            'MANUFACTURE': building_data.get('manufactFormulas', {}),
        }
        item_names = {
            str(item_id): item_info.get('name', str(item_id))
            for item_id, item_info in items_data.items()
            if isinstance(item_info, dict)
        }

        recipes: Dict[str, ItemRecipe] = {}

        for item_id, item_info in items_data.items():
            if not isinstance(item_info, dict):
                continue

            for product_info in item_info.get('buildingProductList') or []:
                room_type = product_info.get('roomType')
                formula_id = str(product_info.get('formulaId', ''))
                formula = formula_tables.get(room_type, {}).get(formula_id)
                if not formula or str(formula.get('itemId')) != str(item_id):
                    continue

                materials = []
                for cost in formula.get('costs') or []:
                    material_id = str(cost.get('id', ''))
                    if not material_id:
                        continue
                    materials.append(RecipeMaterial(
                        item_id=material_id,
                        item_name=item_names.get(material_id, material_id),
                        count=int(cost.get('count', 0) or 0)
                    ))

                if materials:
                    recipes[str(item_id)] = ItemRecipe(
                        cost_gold=int(formula.get('goldCost', 0) or 0),
                        materials=materials
                    )
                    break

        return recipes

    def _parse_item(
        self,
        item_id: str,
        data: Dict[str, Any],
        recipe: Optional[ItemRecipe] = None
    ) -> Optional[Item]:
        """解析物品数据"""
        try:
            return Item(
                id=item_id,
                name=data.get('name', item_id),
                source=DataSource.GITHUB,
                version=self._version,
                item_type=ItemType(data.get('itemType', 'NONE')),
                rarity=ItemRarity.from_string(data.get('rarity', 'TIER_3')),
                description=data.get('description', ''),
                icon_id=data.get('iconId', ''),
                override_bkg=data.get('overrideBkg'),
                stack_icon_id=data.get('stackIconId'),
                usage=data.get('usage', ''),
                obtain_approach=data.get('obtainApproach', ''),
                classify_type=data.get('classifyType', 'NONE'),
                sort_id=data.get('sortId', 0),
                hide_in_item_get=data.get('hideInItemGet', False),
                recipe=recipe,
                raw_data=data
            )

        except Exception as e:
            # 只在非ValueError时记录错误（ValueError通常是未知的item_type，已回退到NONE）
            if not isinstance(e, ValueError):
                logger.warning(f"解析物品 {item_id} 数据失败: {e}")
            return None

    def get_enemies(self, progress_callback: Optional[Callable[[int, int], None]] = None) -> List[Enemy]:
        """
        获取所有敌人数据

        从敌人手册获取基础信息，从敌人数据库获取属性数据

        Args:
            progress_callback: 进度回调函数

        Returns:
            敌人列表
        """
        # 加载敌人手册数据
        handbook_data = self._load_json('enemy')
        if not handbook_data:
            return []

        # 加载敌人数据库（属性数据）
        enemy_db_path = self.config.local_path / self.config.data_root / 'gamedata/levels/enemydata/enemy_database.json'
        enemy_db = {}
        if enemy_db_path.exists():
            try:
                with open(enemy_db_path, 'r', encoding='utf-8') as f:
                    db_content = json.load(f)
                    # 构建敌人ID到属性的映射
                    for entry in db_content.get('enemies', []):
                        enemy_id = entry.get('Key')
                        if enemy_id:
                            # 获取第一个level的数据（通常是level 0）
                            values = entry.get('Value', [])
                            if values:
                                enemy_db[enemy_id] = values[0].get('enemyData', {})
            except Exception as e:
                logger.warning(f"加载敌人数据库失败: {e}")

        enemies = []
        # 敌人数据在 enemyData 字段中
        enemies_data = handbook_data.get('enemyData', {})

        if isinstance(enemies_data, dict):
            items = list(enemies_data.items())
        else:
            items = []

        total = len(items)

        for idx, (enemy_id, enemy_info) in enumerate(items):
            try:
                if progress_callback:
                    progress_callback(idx + 1, total)

                # 合并手册数据和属性数据
                merged_data = enemy_info.copy()
                if enemy_id in enemy_db:
                    merged_data['attributes'] = enemy_db[enemy_id].get('attributes', {})
                    # 合并其他可能有用的字段
                    for key in ['skills', 'talentBlackboard']:
                        if key in enemy_db[enemy_id]:
                            merged_data[key] = enemy_db[enemy_id][key]

                enemy = self._parse_enemy(enemy_id, merged_data)
                if enemy:
                    enemies.append(enemy)

            except Exception as e:
                logger.warning(f"解析敌人 {enemy_id} 失败: {e}")
                continue

        logger.info(f"加载了 {len(enemies)} 个敌人")
        return enemies

    def _parse_enemy(self, enemy_id: str, data: Dict[str, Any]) -> Optional[Enemy]:
        """解析敌人数据"""
        try:
            # 获取基础属性 - 处理嵌套结构
            attributes = data.get('attributes', {})

            def get_attr_value(attr_name: str, default=0):
                """获取属性值，处理嵌套结构"""
                attr = attributes.get(attr_name, {})
                if isinstance(attr, dict):
                    return attr.get('m_value', default)
                return attr if attr is not None else default

            return Enemy(
                id=enemy_id,
                name=data.get('name', enemy_id),
                source=DataSource.GITHUB,
                version=self._version,
                enemy_level=EnemyLevel(data.get('enemyLevel', 'NORMAL')),
                description=data.get('description', ''),
                max_hp=int(get_attr_value('maxHp', 0)),
                atk=int(get_attr_value('atk', 0)),
                def_=int(get_attr_value('def', 0)),
                magic_resistance=float(get_attr_value('magicResistance', 0)),
                move_speed=float(get_attr_value('moveSpeed', 1)),
                attack_speed=float(get_attr_value('attackSpeed', 100)),
                base_attack_time=float(get_attr_value('baseAttackTime', 1)),
                hp_recovery_per_sec=float(get_attr_value('hpRecoveryPerSec', 0)),
                mass_level=int(get_attr_value('massLevel', 0)),
                taunt_level=int(get_attr_value('tauntLevel', 0)),
                stun_immune=data.get('stunImmune', False),
                silence_immune=data.get('silenceImmune', False),
                sleep_immune=data.get('sleepImmune', False),
                frozen_immune=data.get('frozenImmune', False),
                abilities=[],  # 需要从其他数据解析
                icon_id=data.get('iconId', ''),
                raw_data=data
            )

        except Exception as e:
            logger.error(f"解析敌人 {enemy_id} 数据失败: {e}")
            return None

    def get_version(self) -> Optional[DataVersion]:
        """获取当前数据版本"""
        return self._version

    def get_stats(self) -> Dict[str, Any]:
        """获取提供者统计信息"""
        return {
            'initialized': self._initialized,
            'last_sync': self._last_sync.isoformat() if self._last_sync else None,
            'version': self._version.to_dict() if self._version else None,
            'cached_data_types': list(self._data_cache.keys()),
            'repo_path': str(self.config.local_path),
            'auto_pull': self.config.auto_pull,
            'pull_interval_hours': self.config.pull_interval_hours
        }
