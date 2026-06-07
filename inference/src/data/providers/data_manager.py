# -*- coding: utf-8 -*-
"""
统一数据管理器

整合GitHub和PRTS两个数据源，提供统一的数据访问接口
支持结构化数据库存储

Author: Data System
Version: 2.0.0
"""

import json
import logging
import pickle
import sqlite3
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Any, Callable, Union
from dataclasses import dataclass, field
import threading

from ..models.base import DataSource, DataVersion, DataCache
from ..models.operator import Operator, OperatorProfession, OperatorRarity, PositionType
from ..models.stage import Stage, StageType, Difficulty
from ..models.item import Item, ItemType
from ..models.enemy import Enemy, EnemyLevel
from ..database import StructuredDatabaseManager
from .github_provider import GitHubDataProvider, SyncConfig
from .prts_provider import PRTSDataProvider, PRTSConfig

logger = logging.getLogger(__name__)


@dataclass
class CacheConfig:
    """缓存配置"""
    enabled: bool = True
    cache_dir: Path = field(default_factory=lambda: Path("cache"))
    memory_cache_ttl: int = 3600  # 内存缓存过期时间（秒）
    disk_cache_ttl: int = 86400  # 磁盘缓存过期时间（秒）
    db_path: Path = field(default_factory=lambda: Path("cache/arknights_data.db"))


@dataclass
class ManagerConfig:
    """管理器配置"""
    github_repo_url: str = "https://github.com/Kengxxiao/ArknightsGameData.git"
    github_repo_path: Path = field(default_factory=lambda: Path("ArknightsGameData"))
    github_data_root: str = "zh_CN"
    github_sparse_paths: Optional[List[str]] = None
    cache: CacheConfig = field(default_factory=CacheConfig)
    github_sync_interval_hours: int = 24
    prefer_online: bool = False  # 是否优先使用在线数据


class DataManager:
    """
    统一数据管理器

    整合GitHub本地数据和PRTS在线数据，提供统一的数据访问接口
    支持结构化数据库存储和复杂查询
    """

    def __init__(self, config: Optional[ManagerConfig] = None):
        """
        初始化数据管理器

        Args:
            config: 管理器配置
        """
        self.config = config or ManagerConfig()

        # 初始化数据提供者
        github_config = SyncConfig(
            repo_url=self.config.github_repo_url,
            local_path=self.config.github_repo_path,
            data_root=self.config.github_data_root,
            pull_interval_hours=self.config.github_sync_interval_hours,
            sparse_paths=self.config.github_sparse_paths
        )
        self._github_provider = GitHubDataProvider(github_config)

        self._prts_provider = PRTSDataProvider()

        # 初始化缓存
        self._cache = DataCache(str(self.config.cache.cache_dir))
        self._db: Optional[sqlite3.Connection] = None
        self._lock = threading.RLock()

        # 内存缓存
        self._memory_cache: Dict[str, Any] = {}
        self._memory_cache_time: Dict[str, datetime] = {}

        # 数据索引
        self._operators_index: Dict[str, Operator] = {}
        self._stages_index: Dict[str, Stage] = {}
        self._items_index: Dict[str, Item] = {}
        self._enemies_index: Dict[str, Enemy] = {}

        # 结构化数据库管理器
        self._structured_db: Optional[StructuredDatabaseManager] = None

        self._initialized = False

    def initialize(self) -> bool:
        """
        初始化数据管理器

        Returns:
            是否初始化成功
        """
        try:
            logger.info("初始化数据管理器...")

            # 创建缓存目录
            self.config.cache.cache_dir.mkdir(parents=True, exist_ok=True)

            # 初始化GitHub提供者
            if not self._github_provider.initialize():
                logger.warning("GitHub数据提供者初始化失败，将使用本地缓存")

            # 初始化PRTS提供者
            if not self._prts_provider.initialize():
                logger.warning("PRTS数据提供者初始化失败")

            # 初始化旧版数据库（向后兼容）
            self._init_database()

            # 初始化结构化数据库
            self._init_structured_database()

            # 加载本地索引
            self._load_indexes()

            self._initialized = True
            logger.info("数据管理器初始化完成")
            return True

        except Exception as e:
            logger.error(f"初始化失败: {e}")
            return False

    def _init_structured_database(self) -> None:
        """初始化结构化数据库"""
        try:
            db_path = self.config.cache.cache_dir / "arknights_structured.db"
            self._structured_db = StructuredDatabaseManager(db_path)
            if self._structured_db.initialize():
                logger.info(f"结构化数据库初始化完成: {db_path}")
            else:
                logger.warning("结构化数据库初始化失败")
                self._structured_db = None
        except Exception as e:
            logger.error(f"初始化结构化数据库失败: {e}")
            self._structured_db = None

    def _init_database(self) -> None:
        """初始化SQLite数据库"""
        try:
            self._db = sqlite3.connect(str(self.config.cache.db_path))
            self._db.row_factory = sqlite3.Row

            # 创建表
            cursor = self._db.cursor()

            # 干员表
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS operators (
                    id TEXT PRIMARY KEY,
                    data TEXT NOT NULL,
                    updated_at TEXT NOT NULL
                )
            ''')

            # 关卡表
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS stages (
                    id TEXT PRIMARY KEY,
                    data TEXT NOT NULL,
                    updated_at TEXT NOT NULL
                )
            ''')

            # 物品表
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS items (
                    id TEXT PRIMARY KEY,
                    data TEXT NOT NULL,
                    updated_at TEXT NOT NULL
                )
            ''')

            # 敌人表
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS enemies (
                    id TEXT PRIMARY KEY,
                    data TEXT NOT NULL,
                    updated_at TEXT NOT NULL
                )
            ''')

            # 版本表
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS version_info (
                    source TEXT PRIMARY KEY,
                    version TEXT NOT NULL,
                    updated_at TEXT NOT NULL
                )
            ''')

            self._db.commit()
            logger.info(f"数据库初始化完成: {self.config.cache.db_path}")

        except Exception as e:
            logger.error(f"数据库初始化失败: {e}")
            raise

    def _load_indexes(self) -> None:
        """加载本地数据索引"""
        if not self._db:
            return

        try:
            cursor = self._db.cursor()

            # 加载干员索引
            cursor.execute('SELECT id, data FROM operators')
            for row in cursor.fetchall():
                operator_data = json.loads(row['data'])
                operator = Operator.from_dict(operator_data)
                self._operators_index[operator.id] = operator

            # 加载关卡索引
            cursor.execute('SELECT id, data FROM stages')
            for row in cursor.fetchall():
                stage_data = json.loads(row['data'])
                stage = Stage.from_dict(stage_data)
                self._stages_index[stage.id] = stage

            # 加载物品索引
            cursor.execute('SELECT id, data FROM items')
            for row in cursor.fetchall():
                item_data = json.loads(row['data'])
                item = Item.from_dict(item_data)
                self._items_index[item.id] = item

            logger.info(
                f"索引加载完成: "
                f"干员={len(self._operators_index)}, "
                f"关卡={len(self._stages_index)}, "
                f"物品={len(self._items_index)}"
            )

        except Exception as e:
            logger.error(f"加载索引失败: {e}")

    def sync_github(self, force: bool = False) -> bool:
        """
        同步GitHub数据

        Args:
            force: 是否强制同步

        Returns:
            是否同步成功
        """
        with self._lock:
            if not self._github_provider.initialize():
                return False

            success = self._github_provider.sync(force=force)
            if success:
                # 重新加载索引
                self._load_indexes()

            return success

    def load_all_data(
        self,
        progress_callback: Optional[Callable[[str, int, int], None]] = None
    ) -> bool:
        """
        加载所有数据

        Args:
            progress_callback: 进度回调函数(type, current, total)

        Returns:
            是否加载成功
        """
        try:
            total_types = 4  # operator, stage, item, enemy
            current_type = 0

            # 加载干员
            current_type += 1
            if progress_callback:
                progress_callback('operator', 0, 1)
            operators = self._github_provider.get_operators(
                progress_callback=lambda c, t: progress_callback('operator', c, t) if progress_callback else None
            )
            for op in operators:
                self._operators_index[op.id] = op
                self._save_to_db('operators', op)
            logger.info(f"加载了 {len(operators)} 个干员")

            # 加载关卡
            current_type += 1
            if progress_callback:
                progress_callback('stage', 0, 1)
            stages = self._github_provider.get_stages(
                progress_callback=lambda c, t: progress_callback('stage', c, t) if progress_callback else None
            )
            for stage in stages:
                self._stages_index[stage.id] = stage
                self._save_to_db('stages', stage)
            logger.info(f"加载了 {len(stages)} 个关卡")

            # 加载物品
            current_type += 1
            if progress_callback:
                progress_callback('item', 0, 1)
            items = self._github_provider.get_items(
                progress_callback=lambda c, t: progress_callback('item', c, t) if progress_callback else None
            )
            for item in items:
                self._items_index[item.id] = item
                self._save_to_db('items', item)
            logger.info(f"加载了 {len(items)} 个物品")

            # 加载敌人
            current_type += 1
            if progress_callback:
                progress_callback('enemy', 0, 1)
            enemies = self._github_provider.get_enemies(
                progress_callback=lambda c, t: progress_callback('enemy', c, t) if progress_callback else None
            )
            for enemy in enemies:
                self._enemies_index[enemy.id] = enemy
                self._save_to_db('enemies', enemy)
            logger.info(f"加载了 {len(enemies)} 个敌人")

            if progress_callback:
                progress_callback('complete', current_type, total_types)

            logger.info("数据加载完成")
            return True

        except Exception as e:
            logger.error(f"加载数据失败: {e}")
            return False

    def load_all_data_structured(
        self,
        progress_callback: Optional[Callable[[str, int, int], None]] = None
    ) -> bool:
        """
        加载所有数据到结构化数据库

        Args:
            progress_callback: 进度回调函数(type, current, total)

        Returns:
            是否加载成功
        """
        if not self._structured_db:
            logger.error("结构化数据库未初始化")
            return False

        try:
            total_types = 4
            current_type = 0

            # 加载干员
            current_type += 1
            if progress_callback:
                progress_callback('operator', 0, 1)
            operators = self._github_provider.get_operators(
                progress_callback=lambda c, t: progress_callback('operator', c, t) if progress_callback else None
            )
            saved_count = 0
            for op in operators:
                if self._structured_db.save_operator(op):
                    saved_count += 1
            logger.info(f"结构化存储: {saved_count}/{len(operators)} 个干员")

            # 加载关卡
            current_type += 1
            if progress_callback:
                progress_callback('stage', 0, 1)
            stages = self._github_provider.get_stages(
                progress_callback=lambda c, t: progress_callback('stage', c, t) if progress_callback else None
            )
            saved_count = 0
            for stage in stages:
                if self._structured_db.save_stage(stage):
                    saved_count += 1
            logger.info(f"结构化存储: {saved_count}/{len(stages)} 个关卡")

            # 加载物品
            current_type += 1
            if progress_callback:
                progress_callback('item', 0, 1)
            items = self._github_provider.get_items(
                progress_callback=lambda c, t: progress_callback('item', c, t) if progress_callback else None
            )
            saved_count = self._save_items_structured(items)
            logger.info(f"结构化存储: {saved_count}/{len(items)} 个物品")

            # 加载敌人
            current_type += 1
            if progress_callback:
                progress_callback('enemy', 0, 1)
            enemies = self._github_provider.get_enemies(
                progress_callback=lambda c, t: progress_callback('enemy', c, t) if progress_callback else None
            )
            saved_count = 0
            for enemy in enemies:
                if self._structured_db.save_enemy(enemy):
                    saved_count += 1
            logger.info(f"结构化存储: {saved_count}/{len(enemies)} 个敌人")

            if progress_callback:
                progress_callback('complete', current_type, total_types)

            # 保存版本信息
            version = self._github_provider.get_version()
            if version:
                self._structured_db.save_version_info(
                    'github',
                    version.version,
                    version.commit_hash or ''
                )

            logger.info("结构化数据加载完成")
            return True

        except Exception as e:
            logger.error(f"加载结构化数据失败: {e}")
            return False

    def _save_items_structured(self, items: List[Item]) -> int:
        """两阶段保存物品，避免配方材料外键依赖保存顺序"""
        if not self._structured_db:
            return 0

        saved_count = 0
        recipes: Dict[str, Any] = {}

        for item in items:
            recipe = getattr(item, 'recipe', None)
            if recipe is not None:
                recipes[item.id] = recipe
                item.recipe = None

            self._items_index[item.id] = item
            if self._structured_db.save_item(item):
                saved_count += 1

            if recipe is not None:
                item.recipe = recipe

        for item in items:
            if item.id in recipes:
                self._structured_db.save_item(item)

        return saved_count

    def load_items_structured(
        self,
        progress_callback: Optional[Callable[[int, int], None]] = None
    ) -> bool:
        """
        只加载物品和合成配方到结构化数据库

        Args:
            progress_callback: 进度回调函数(current, total)

        Returns:
            是否加载成功
        """
        if not self._structured_db:
            logger.error("结构化数据库未初始化")
            return False

        try:
            items = self._github_provider.get_items(progress_callback=progress_callback)
            saved_count = self._save_items_structured(items)

            logger.info(f"结构化存储物品: {saved_count}/{len(items)} 个")
            return saved_count > 0

        except Exception as e:
            logger.error(f"加载结构化物品失败: {e}")
            return False

    def _save_to_db(self, table: str, obj: Any) -> None:
        """保存对象到数据库"""
        if not self._db:
            return

        try:
            cursor = self._db.cursor()
            cursor.execute(
                f'INSERT OR REPLACE INTO {table} (id, data, updated_at) VALUES (?, ?, ?)',
                (obj.id, json.dumps(obj.to_dict(), ensure_ascii=False), datetime.now().isoformat())
            )
            self._db.commit()
        except Exception as e:
            logger.warning(f"保存到数据库失败: {e}")

    def get_operator(self, operator_id: str) -> Optional[Operator]:
        """
        获取干员数据

        Args:
            operator_id: 干员ID

        Returns:
            干员数据或None
        """
        # 先检查内存缓存
        cache_key = f"operator:{operator_id}"
        if cache_key in self._memory_cache:
            if self._is_memory_cache_valid(cache_key):
                return self._memory_cache[cache_key]

        # 检查索引
        if operator_id in self._operators_index:
            operator = self._operators_index[operator_id]
            self._memory_cache[cache_key] = operator
            self._memory_cache_time[cache_key] = datetime.now()
            return operator

        # 尝试从PRTS获取
        if self._prts_provider._initialized:
            prts_info = self._prts_provider.get_operator_info(operator_id)
            if prts_info:
                # 尝试创建或更新本地数据
                if operator_id in self._operators_index:
                    operator = self._operators_index[operator_id]
                    operator.metadata['prts_info'] = prts_info
                    return operator

        return None

    def get_operators(
        self,
        filter_func: Optional[Callable[[Operator], bool]] = None,
        sort_key: Optional[Callable[[Operator], Any]] = None
    ) -> List[Operator]:
        """
        获取干员列表

        Args:
            filter_func: 过滤函数
            sort_key: 排序键函数

        Returns:
            干员列表
        """
        operators = list(self._operators_index.values())

        if filter_func:
            operators = [op for op in operators if filter_func(op)]

        if sort_key:
            operators.sort(key=sort_key)

        return operators

    def get_stage(self, stage_id: str) -> Optional[Stage]:
        """
        获取关卡数据

        Args:
            stage_id: 关卡ID

        Returns:
            关卡数据或None
        """
        cache_key = f"stage:{stage_id}"
        if cache_key in self._memory_cache and self._is_memory_cache_valid(cache_key):
            return self._memory_cache[cache_key]

        if stage_id in self._stages_index:
            stage = self._stages_index[stage_id]
            self._memory_cache[cache_key] = stage
            self._memory_cache_time[cache_key] = datetime.now()
            return stage

        return None

    def get_stages(
        self,
        filter_func: Optional[Callable[[Stage], bool]] = None,
        sort_key: Optional[Callable[[Stage], Any]] = None
    ) -> List[Stage]:
        """
        获取关卡列表

        Args:
            filter_func: 过滤函数
            sort_key: 排序键函数

        Returns:
            关卡列表
        """
        stages = list(self._stages_index.values())

        if filter_func:
            stages = [s for s in stages if filter_func(s)]

        if sort_key:
            stages.sort(key=sort_key)

        return stages

    def get_item(self, item_id: str) -> Optional[Item]:
        """
        获取物品数据

        Args:
            item_id: 物品ID

        Returns:
            物品数据或None
        """
        cache_key = f"item:{item_id}"
        if cache_key in self._memory_cache and self._is_memory_cache_valid(cache_key):
            return self._memory_cache[cache_key]

        if item_id in self._items_index:
            item = self._items_index[item_id]
            self._memory_cache[cache_key] = item
            self._memory_cache_time[cache_key] = datetime.now()
            return item

        return None

    def get_items(
        self,
        filter_func: Optional[Callable[[Item], bool]] = None,
        sort_key: Optional[Callable[[Item], Any]] = None
    ) -> List[Item]:
        """
        获取物品列表

        Args:
            filter_func: 过滤函数
            sort_key: 排序键函数

        Returns:
            物品列表
        """
        items = list(self._items_index.values())

        if filter_func:
            items = [i for i in items if filter_func(i)]

        if sort_key:
            items.sort(key=sort_key)

        return items

    # ==================== 结构化数据库查询接口 ====================

    @property
    def structured_db(self) -> Optional[StructuredDatabaseManager]:
        """获取结构化数据库管理器"""
        return self._structured_db

    def query_operators_structured(
        self,
        profession: Optional[OperatorProfession] = None,
        rarity: Optional[OperatorRarity] = None,
        position: Optional[PositionType] = None,
        min_rarity: Optional[int] = None,
        max_rarity: Optional[int] = None,
        nation_id: Optional[str] = None,
        team_id: Optional[int] = None,
        is_robot: Optional[bool] = None,
        limit: int = 100,
        offset: int = 0
    ) -> List[Dict[str, Any]]:
        """
        使用结构化数据库查询干员

        Args:
            profession: 职业筛选
            rarity: 稀有度筛选
            position: 位置筛选
            min_rarity: 最小稀有度
            max_rarity: 最大稀有度
            nation_id: 国家筛选
            team_id: 团队筛选
            is_robot: 是否机器人
            limit: 返回数量限制
            offset: 偏移量

        Returns:
            干员列表
        """
        if not self._structured_db:
            logger.warning("结构化数据库未初始化")
            return []
        return self._structured_db.query_operators(
            profession=profession,
            rarity=rarity,
            position=position,
            min_rarity=min_rarity,
            max_rarity=max_rarity,
            nation_id=nation_id,
            team_id=team_id,
            is_robot=is_robot,
            limit=limit,
            offset=offset
        )

    def query_stages_structured(
        self,
        stage_type: Optional[StageType] = None,
        zone_id: Optional[str] = None,
        difficulty: Optional[Difficulty] = None,
        min_ap_cost: Optional[int] = None,
        max_ap_cost: Optional[int] = None,
        can_practice: Optional[bool] = None,
        limit: int = 100,
        offset: int = 0
    ) -> List[Dict[str, Any]]:
        """
        使用结构化数据库查询关卡

        Args:
            stage_type: 关卡类型
            zone_id: 区域ID
            difficulty: 难度
            min_ap_cost: 最小理智消耗
            max_ap_cost: 最大理智消耗
            can_practice: 是否可演习
            limit: 返回数量限制
            offset: 偏移量

        Returns:
            关卡列表
        """
        if not self._structured_db:
            logger.warning("结构化数据库未初始化")
            return []
        return self._structured_db.query_stages(
            stage_type=stage_type,
            zone_id=zone_id,
            difficulty=difficulty,
            min_ap_cost=min_ap_cost,
            max_ap_cost=max_ap_cost,
            can_practice=can_practice,
            limit=limit,
            offset=offset
        )

    def query_items_structured(
        self,
        item_type: Optional[ItemType] = None,
        rarity: Optional[int] = None,
        min_rarity: Optional[int] = None,
        max_rarity: Optional[int] = None,
        is_material: Optional[bool] = None,
        is_exp_card: Optional[bool] = None,
        limit: int = 100,
        offset: int = 0
    ) -> List[Dict[str, Any]]:
        """
        使用结构化数据库查询物品

        Args:
            item_type: 物品类型
            rarity: 稀有度
            min_rarity: 最小稀有度
            max_rarity: 最大稀有度
            is_material: 是否为材料
            is_exp_card: 是否为经验卡
            limit: 返回数量限制
            offset: 偏移量

        Returns:
            物品列表
        """
        if not self._structured_db:
            logger.warning("结构化数据库未初始化")
            return []
        return self._structured_db.query_items(
            item_type=item_type,
            rarity=rarity,
            min_rarity=min_rarity,
            max_rarity=max_rarity,
            is_material=is_material,
            is_exp_card=is_exp_card,
            limit=limit,
            offset=offset
        )

    def query_enemies_structured(
        self,
        enemy_level: Optional[EnemyLevel] = None,
        min_hp: Optional[int] = None,
        max_hp: Optional[int] = None,
        min_atk: Optional[int] = None,
        max_atk: Optional[int] = None,
        limit: int = 100,
        offset: int = 0
    ) -> List[Dict[str, Any]]:
        """
        使用结构化数据库查询敌人

        Args:
            enemy_level: 敌人等级
            min_hp: 最小生命值
            max_hp: 最大生命值
            min_atk: 最小攻击力
            max_atk: 最大攻击力
            limit: 返回数量限制
            offset: 偏移量

        Returns:
            敌人列表
        """
        if not self._structured_db:
            logger.warning("结构化数据库未初始化")
            return []
        return self._structured_db.query_enemies(
            enemy_level=enemy_level,
            min_hp=min_hp,
            max_hp=max_hp,
            min_atk=min_atk,
            max_atk=max_atk,
            limit=limit,
            offset=offset
        )

    def get_material_tree(self, item_id: str, depth: int = 3) -> Dict[str, Any]:
        """
        获取材料合成树

        Args:
            item_id: 物品ID
            depth: 递归深度

        Returns:
            材料树
        """
        if not self._structured_db:
            logger.warning("结构化数据库未初始化")
            return {}
        return self._structured_db.get_material_tree(item_id, depth)

    def get_stages_by_drop_item(self, item_id: str) -> List[Dict[str, Any]]:
        """
        查询掉落指定物品的所有关卡

        Args:
            item_id: 物品ID

        Returns:
            关卡列表
        """
        if not self._structured_db:
            logger.warning("结构化数据库未初始化")
            return []
        return self._structured_db.get_stages_by_drop_item(item_id)

    def get_structured_stats(self) -> Dict[str, Any]:
        """获取结构化数据库统计信息"""
        if not self._structured_db:
            return {'error': '结构化数据库未初始化'}
        return self._structured_db.get_statistics()

    def search_prts(self, query: str, limit: int = 10) -> List[Dict[str, str]]:
        """
        在PRTS Wiki搜索

        Args:
            query: 搜索关键词
            limit: 返回结果数量

        Returns:
            搜索结果列表
        """
        return self._prts_provider.search(query, limit)

    def get_prts_page(self, title: str) -> Optional[str]:
        """
        获取PRTS Wiki页面内容

        Args:
            title: 页面标题

        Returns:
            页面内容或None
        """
        return self._prts_provider.get_page_content(title)

    def _is_memory_cache_valid(self, key: str) -> bool:
        """检查内存缓存是否有效"""
        if key not in self._memory_cache_time:
            return False

        elapsed = (datetime.now() - self._memory_cache_time[key]).total_seconds()
        return elapsed <= self.config.cache.memory_cache_ttl

    def clear_cache(self) -> None:
        """清空所有缓存"""
        with self._lock:
            self._memory_cache.clear()
            self._memory_cache_time.clear()
            logger.info("内存缓存已清空")

    def get_stats(self) -> Dict[str, Any]:
        """获取统计信息"""
        stats = {
            'initialized': self._initialized,
            'operators_count': len(self._operators_index),
            'stages_count': len(self._stages_index),
            'items_count': len(self._items_index),
            'enemies_count': len(self._enemies_index),
            'memory_cache_entries': len(self._memory_cache),
            'github_stats': self._github_provider.get_stats(),
            'prts_stats': self._prts_provider.get_stats()
        }

        # 添加结构化数据库统计
        if self._structured_db:
            stats['structured_db'] = self._structured_db.get_statistics()

        return stats

    def shutdown(self) -> None:
        """关闭数据管理器"""
        if self._db:
            self._db.close()
            self._db = None

        if self._structured_db:
            self._structured_db.close()
            self._structured_db = None

        logger.info("数据管理器已关闭")
