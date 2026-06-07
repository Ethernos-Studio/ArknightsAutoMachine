# -*- coding: utf-8 -*-
"""
结构化数据库管理器

提供完整的CRUD操作和复杂查询支持

Author: Data System
Version: 1.0.0
"""

import json
import logging
import sqlite3
from contextlib import contextmanager
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Any, Callable, Tuple, Union
from dataclasses import asdict

from .schema import DatabaseSchema
from ..models.base import DataSource
from ..models.operator import (
    Operator, OperatorPhase, OperatorSkill, OperatorTalent,
    OperatorProfession, OperatorRarity, PositionType
)
from ..models.stage import Stage, StageDrop, StageType, Difficulty
from ..models.item import Item, ItemType
from ..models.enemy import Enemy, EnemyLevel, EnemyAbility

logger = logging.getLogger(__name__)


class StructuredDatabaseManager:
    """
    结构化数据库管理器

    提供完全结构化的数据存储和查询能力
    """

    def __init__(self, db_path: Union[str, Path] = "data/arknights_structured.db"):
        """
        初始化数据库管理器

        Args:
            db_path: 数据库文件路径
        """
        self.db_path = Path(db_path)
        self.db_path.parent.mkdir(parents=True, exist_ok=True)
        self._connection: Optional[sqlite3.Connection] = None

    def _get_connection(self) -> sqlite3.Connection:
        """获取数据库连接"""
        if self._connection is None:
            self._connection = sqlite3.connect(str(self.db_path))
            self._connection.row_factory = sqlite3.Row
            # 启用外键支持
            self._connection.execute("PRAGMA foreign_keys = ON")
            # 启用WAL模式提高并发性能
            self._connection.execute("PRAGMA journal_mode = WAL")
        return self._connection

    @contextmanager
    def _transaction(self):
        """事务上下文管理器"""
        conn = self._get_connection()
        try:
            yield conn
            conn.commit()
        except Exception as e:
            conn.rollback()
            raise e

    def initialize(self) -> bool:
        """
        初始化数据库结构

        Returns:
            是否初始化成功
        """
        try:
            conn = self._get_connection()
            cursor = conn.cursor()

            # 创建所有表
            for statement in DatabaseSchema.get_create_statements():
                cursor.execute(statement)

            # 创建所有索引
            for statement in DatabaseSchema.get_index_statements():
                cursor.execute(statement)

            conn.commit()
            logger.info(f"数据库初始化完成: {self.db_path}")
            return True

        except Exception as e:
            logger.error(f"数据库初始化失败: {e}")
            return False

    def close(self):
        """关闭数据库连接"""
        if self._connection:
            self._connection.close()
            self._connection = None

    # ==================== 干员操作 ====================

    def save_operator(self, operator: Operator) -> bool:
        """
        保存干员数据

        Args:
            operator: 干员对象

        Returns:
            是否保存成功
        """
        try:
            with self._transaction() as conn:
                cursor = conn.cursor()

                # 安全获取字段值
                def safe_get(obj, attr, default=''):
                    try:
                        val = getattr(obj, attr, default)
                        return val if val is not None else default
                    except:
                        return default

                def safe_bool(obj, attr, default=False):
                    try:
                        val = getattr(obj, attr, default)
                        return bool(val) if val is not None else default
                    except:
                        return default

                # 保存主表
                cursor.execute('''
                    INSERT OR REPLACE INTO operators (
                        id, name, appellation, profession, rarity, position,
                        tag_list, description, max_phases,
                        can_use_general_potential_item, can_use_activity_potential_item,
                        potential_item_id, activity_potential_item_id,
                        team_id, display_number, group_id, nation_id,
                        is_not_obtainable, is_sp_char, is_robot,
                        source, version, raw_data, updated_at
                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                ''', (
                    safe_get(operator, 'id'),
                    safe_get(operator, 'name'),
                    safe_get(operator, 'appellation'),
                    safe_get(operator.profession, 'value', str(operator.profession)) if hasattr(operator, 'profession') else 'WARRIOR',
                    safe_get(operator.rarity, 'value', 3) if hasattr(operator, 'rarity') else 3,
                    safe_get(operator.position, 'value', 'MELEE') if hasattr(operator, 'position') else 'MELEE',
                    json.dumps(safe_get(operator, 'tag_list', []), ensure_ascii=False),
                    safe_get(operator, 'description'),
                    len(safe_get(operator, 'phases', [])),
                    safe_bool(operator, 'can_use_general_potential_item'),
                    safe_bool(operator, 'can_use_activity_potential_item'),
                    safe_get(operator, 'potential_item_id'),
                    safe_get(operator, 'activity_potential_item_id'),
                    safe_get(operator, 'team_id'),
                    safe_get(operator, 'display_number'),
                    safe_get(operator, 'group_id'),
                    safe_get(operator, 'nation_id'),
                    safe_bool(operator, 'is_not_obtainable'),
                    safe_bool(operator, 'is_sp_char'),
                    safe_bool(operator, 'is_robot'),
                    safe_get(operator.source, 'name', 'LOCAL_CACHE') if hasattr(operator, 'source') else 'LOCAL_CACHE',
                    safe_get(operator.version, 'version', '') if hasattr(operator, 'version') else '',
                    json.dumps(operator.to_dict(), ensure_ascii=False),
                    datetime.now().isoformat()
                ))

                # 删除旧的关联数据
                cursor.execute('DELETE FROM operator_phases WHERE operator_id = ?', (operator.id,))
                cursor.execute('DELETE FROM operator_skills WHERE operator_id = ?', (operator.id,))
                cursor.execute('DELETE FROM operator_talents WHERE operator_id = ?', (operator.id,))
                cursor.execute('DELETE FROM operator_potentials WHERE operator_id = ?', (operator.id,))

                # 保存精英化阶段
                for phase in getattr(operator, 'phases', []):
                    cursor.execute('''
                        INSERT INTO operator_phases (
                            operator_id, phase_index, max_level, max_hp, atk, def,
                            magic_resistance, cost, block_count, attack_speed, respawn_time
                        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                    ''', (
                        operator.id,
                        phase.phase_index,
                        phase.max_level,
                        phase.max_hp,
                        phase.atk,
                        phase.def_,
                        phase.magic_resistance,
                        phase.cost,
                        phase.block_count,
                        phase.attack_speed,
                        phase.respawn_time
                    ))

                # 保存技能
                for skill in getattr(operator, 'skills', []):
                    cursor.execute('''
                        INSERT INTO operator_skills (
                            operator_id, skill_id, skill_name, description,
                            sp_cost, sp_initial, duration, skill_type
                        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                    ''', (
                        operator.id,
                        skill.skill_id,
                        skill.skill_name,
                        skill.description,
                        skill.sp_cost,
                        skill.sp_initial,
                        skill.duration,
                        getattr(skill, 'skill_type', '')
                    ))

                # 保存天赋
                for talent in getattr(operator, 'talents', []):
                    cursor.execute('''
                        INSERT INTO operator_talents (
                            operator_id, talent_id, talent_name, description,
                            unlock_phase, unlock_level, required_potential_rank
                        ) VALUES (?, ?, ?, ?, ?, ?, ?)
                    ''', (
                        operator.id,
                        talent.talent_id,
                        talent.talent_name,
                        talent.description,
                        talent.unlock_phase,
                        talent.unlock_level,
                        getattr(talent, 'required_potential_rank', 0)
                    ))

                # 保存潜能
                for potential in getattr(operator, 'potentials', []):
                    cursor.execute('''
                        INSERT INTO operator_potentials (
                            operator_id, rank, description, buff_id
                        ) VALUES (?, ?, ?, ?)
                    ''', (
                        operator.id,
                        potential.rank,
                        potential.description,
                        getattr(potential, 'buff_id', '')
                    ))

            return True

        except Exception as e:
            logger.error(f"保存干员 {operator.id} 失败: {e}")
            return False

    def get_operator(self, operator_id: str) -> Optional[Operator]:
        """
        获取干员数据

        Args:
            operator_id: 干员ID

        Returns:
            干员对象或None
        """
        try:
            conn = self._get_connection()
            cursor = conn.cursor()

            # 查询主表
            cursor.execute('SELECT * FROM operators WHERE id = ?', (operator_id,))
            row = cursor.fetchone()
            if not row:
                return None

            # 从raw_data重建对象
            raw_data = json.loads(row['raw_data'])
            return Operator.from_dict(raw_data)

        except Exception as e:
            logger.error(f"获取干员 {operator_id} 失败: {e}")
            return None

    def query_operators(
        self,
        profession: Optional[Union[OperatorProfession, str]] = None,
        rarity: Optional[Union[OperatorRarity, int]] = None,
        position: Optional[Union[PositionType, str]] = None,
        min_rarity: Optional[int] = None,
        max_rarity: Optional[int] = None,
        nation_id: Optional[str] = None,
        team_id: Optional[int] = None,
        is_robot: Optional[bool] = None,
        limit: int = 100,
        offset: int = 0
    ) -> List[Dict[str, Any]]:
        """
        查询干员

        Args:
            profession: 职业筛选(枚举或字符串)
            rarity: 稀有度筛选(枚举或整数)
            position: 位置筛选(枚举或字符串)
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
        try:
            conn = self._get_connection()
            cursor = conn.cursor()

            conditions = []
            params = []

            if profession:
                conditions.append("profession = ?")
                prof_value = profession.value if isinstance(profession, OperatorProfession) else str(profession)
                params.append(prof_value)
            if rarity:
                conditions.append("rarity = ?")
                rarity_value = rarity.value if isinstance(rarity, OperatorRarity) else int(rarity)
                params.append(rarity_value)
            if position:
                conditions.append("position = ?")
                pos_value = position.value if isinstance(position, PositionType) else str(position)
                params.append(pos_value)
            if min_rarity is not None:
                conditions.append("rarity >= ?")
                params.append(min_rarity)
            if max_rarity is not None:
                conditions.append("rarity <= ?")
                params.append(max_rarity)
            if nation_id:
                conditions.append("nation_id = ?")
                params.append(nation_id)
            if team_id is not None:
                conditions.append("team_id = ?")
                params.append(team_id)
            if is_robot is not None:
                conditions.append("is_robot = ?")
                params.append(1 if is_robot else 0)

            where_clause = " AND ".join(conditions) if conditions else "1=1"

            cursor.execute(f'''
                SELECT id, name, profession, rarity, position, max_phases,
                       nation_id, team_id, is_robot, updated_at
                FROM operators
                WHERE {where_clause}
                ORDER BY rarity DESC, name
                LIMIT ? OFFSET ?
            ''', (*params, limit, offset))

            return [dict(row) for row in cursor.fetchall()]

        except Exception as e:
            logger.error(f"查询干员失败: {e}")
            return []

    def get_operator_stats(self, operator_id: str, phase: int = 2) -> Optional[Dict[str, Any]]:
        """
        获取干员指定阶段的属性

        Args:
            operator_id: 干员ID
            phase: 精英化阶段

        Returns:
            属性字典
        """
        try:
            conn = self._get_connection()
            cursor = conn.cursor()

            cursor.execute('''
                SELECT * FROM operator_phases
                WHERE operator_id = ? AND phase_index = ?
            ''', (operator_id, phase))

            row = cursor.fetchone()
            return dict(row) if row else None

        except Exception as e:
            logger.error(f"获取干员属性失败: {e}")
            return None

    def get_operator_skills(self, operator_id: str) -> List[Dict[str, Any]]:
        """获取干员技能列表"""
        try:
            conn = self._get_connection()
            cursor = conn.cursor()

            cursor.execute('''
                SELECT * FROM operator_skills
                WHERE operator_id = ?
                ORDER BY skill_id
            ''', (operator_id,))

            return [dict(row) for row in cursor.fetchall()]

        except Exception as e:
            logger.error(f"获取干员技能失败: {e}")
            return []

    # ==================== 关卡操作 ====================

    def save_stage(self, stage: Stage) -> bool:
        """
        保存关卡数据

        Args:
            stage: 关卡对象

        Returns:
            是否保存成功
        """
        try:
            with self._transaction() as conn:
                cursor = conn.cursor()

                # 保存主表
                cursor.execute('''
                    INSERT OR REPLACE INTO stages (
                        id, name, code, stage_type, difficulty, description,
                        zone_id, zone_name, level_id,
                        ap_cost, ap_fail_return, exp_gain, gold_gain,
                        can_practice, can_battle_replay, can_continuous_battle,
                        is_hard_stage, is_story_only, is_stage_patch,
                        danger_level, loading_pic_id, main_stage_id, boss_id,
                        source, version, raw_data, updated_at
                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                ''', (
                    stage.id,
                    stage.name,
                    getattr(stage, 'code', ''),
                    stage.stage_type.value if hasattr(stage.stage_type, 'value') else str(stage.stage_type),
                    stage.difficulty.value if hasattr(stage.difficulty, 'value') else str(stage.difficulty),
                    stage.description,
                    stage.zone_id,
                    getattr(stage, 'zone_name', ''),
                    getattr(stage, 'level_id', ''),
                    stage.ap_cost,
                    stage.ap_fail_return,
                    stage.exp_gain,
                    stage.gold_gain,
                    stage.can_practice,
                    stage.can_battle_replay,
                    getattr(stage, 'can_continuous_battle', False),
                    getattr(stage, 'is_hard_stage', False),
                    getattr(stage, 'is_story_only', False),
                    getattr(stage, 'is_stage_patch', False),
                    getattr(stage, 'danger_level', ''),
                    getattr(stage, 'loading_pic_id', ''),
                    getattr(stage, 'main_stage_id', ''),
                    getattr(stage, 'boss_id', ''),
                    stage.source.name if hasattr(stage.source, 'name') else str(stage.source),
                    stage.version.version if stage.version else '',
                    json.dumps(stage.to_dict(), ensure_ascii=False),
                    datetime.now().isoformat()
                ))

                # 删除旧的关联数据
                cursor.execute('DELETE FROM stage_drops WHERE stage_id = ?', (stage.id,))
                cursor.execute('DELETE FROM stage_conditions WHERE stage_id = ?', (stage.id,))

                # 保存掉落
                for drop in getattr(stage, 'drops', []):
                    cursor.execute('''
                        INSERT INTO stage_drops (
                            stage_id, item_id, item_name, drop_type,
                            occ_percent, count, weight
                        ) VALUES (?, ?, ?, ?, ?, ?, ?)
                    ''', (
                        stage.id,
                        drop.item_id,
                        drop.item_name,
                        drop.drop_type,
                        drop.occ_percent,
                        drop.count,
                        getattr(drop, 'weight', 0)
                    ))

                # 保存解锁条件
                for condition in getattr(stage, 'unlock_conditions', []):
                    cursor.execute('''
                        INSERT INTO stage_conditions (
                            stage_id, condition_type, condition_value
                        ) VALUES (?, ?, ?)
                    ''', (
                        stage.id,
                        condition.condition_type,
                        condition.value
                    ))

            return True

        except Exception as e:
            logger.error(f"保存关卡 {stage.id} 失败: {e}")
            return False

    def get_stage(self, stage_id: str) -> Optional[Stage]:
        """获取关卡数据"""
        try:
            conn = self._get_connection()
            cursor = conn.cursor()

            cursor.execute('SELECT raw_data FROM stages WHERE id = ?', (stage_id,))
            row = cursor.fetchone()
            if not row:
                return None

            raw_data = json.loads(row['raw_data'])
            return Stage.from_dict(raw_data)

        except Exception as e:
            logger.error(f"获取关卡 {stage_id} 失败: {e}")
            return None

    def query_stages(
        self,
        stage_type: Optional[Union[StageType, str]] = None,
        zone_id: Optional[str] = None,
        difficulty: Optional[Union[Difficulty, str]] = None,
        min_ap_cost: Optional[int] = None,
        max_ap_cost: Optional[int] = None,
        can_practice: Optional[bool] = None,
        has_drops: Optional[bool] = None,
        limit: int = 100,
        offset: int = 0
    ) -> List[Dict[str, Any]]:
        """
        查询关卡

        Args:
            stage_type: 关卡类型(枚举或字符串)
            zone_id: 区域ID
            difficulty: 难度(枚举或字符串)
            min_ap_cost: 最小理智消耗
            max_ap_cost: 最大理智消耗
            can_practice: 是否可演习
            has_drops: 是否有掉落
            limit: 返回数量限制
            offset: 偏移量

        Returns:
            关卡列表
        """
        try:
            conn = self._get_connection()
            cursor = conn.cursor()

            conditions = []
            params = []

            if stage_type:
                conditions.append("stage_type = ?")
                st_value = stage_type.value if isinstance(stage_type, StageType) else str(stage_type)
                params.append(st_value)
            if zone_id:
                conditions.append("zone_id = ?")
                params.append(zone_id)
            if difficulty:
                conditions.append("difficulty = ?")
                diff_value = difficulty.value if isinstance(difficulty, Difficulty) else str(difficulty)
                params.append(diff_value)
            if min_ap_cost is not None:
                conditions.append("ap_cost >= ?")
                params.append(min_ap_cost)
            if max_ap_cost is not None:
                conditions.append("ap_cost <= ?")
                params.append(max_ap_cost)
            if can_practice is not None:
                conditions.append("can_practice = ?")
                params.append(1 if can_practice else 0)

            where_clause = " AND ".join(conditions) if conditions else "1=1"

            cursor.execute(f'''
                SELECT id, name, code, stage_type, difficulty, zone_id,
                       ap_cost, exp_gain, gold_gain, can_practice,
                       can_battle_replay, updated_at
                FROM stages
                WHERE {where_clause}
                ORDER BY code, name
                LIMIT ? OFFSET ?
            ''', (*params, limit, offset))

            return [dict(row) for row in cursor.fetchall()]

        except Exception as e:
            logger.error(f"查询关卡失败: {e}")
            return []

    def get_stage_drops(self, stage_id: str) -> List[Dict[str, Any]]:
        """获取关卡掉落"""
        try:
            conn = self._get_connection()
            cursor = conn.cursor()

            cursor.execute('''
                SELECT * FROM stage_drops
                WHERE stage_id = ?
                ORDER BY drop_type, item_name
            ''', (stage_id,))

            return [dict(row) for row in cursor.fetchall()]

        except Exception as e:
            logger.error(f"获取关卡掉落失败: {e}")
            return []

    def get_stages_by_drop_item(self, item_id: str) -> List[Dict[str, Any]]:
        """
        查询掉落指定物品的所有关卡

        Args:
            item_id: 物品ID

        Returns:
            关卡列表
        """
        try:
            conn = self._get_connection()
            cursor = conn.cursor()

            cursor.execute('''
                SELECT s.id, s.name, s.code, s.ap_cost, sd.drop_type, sd.occ_percent
                FROM stages s
                JOIN stage_drops sd ON s.id = sd.stage_id
                WHERE sd.item_id = ?
                ORDER BY s.ap_cost, s.code
            ''', (item_id,))

            return [dict(row) for row in cursor.fetchall()]

        except Exception as e:
            logger.error(f"查询掉落关卡失败: {e}")
            return []

    # ==================== 物品操作 ====================

    def save_item(self, item: Item) -> bool:
        """
        保存物品数据

        Args:
            item: 物品对象

        Returns:
            是否保存成功
        """
        try:
            with self._transaction() as conn:
                cursor = conn.cursor()

                # 安全获取字段值
                def safe_get(obj, attr, default=''):
                    try:
                        val = getattr(obj, attr, default)
                        return val if val is not None else default
                    except:
                        return default

                def safe_bool(obj, attr, default=False):
                    try:
                        val = getattr(obj, attr, default)
                        return bool(val) if val is not None else default
                    except:
                        return default

                def safe_int(obj, attr, default=0):
                    try:
                        val = getattr(obj, attr, default)
                        if val is None:
                            return default
                        # 处理枚举类型
                        if hasattr(val, 'value'):
                            val = val.value
                        return int(val)
                    except:
                        return default

                # 保存主表
                cursor.execute('''
                    INSERT OR REPLACE INTO items (
                        id, name, item_type, description, rarity, icon_id,
                        usage, obtain_approach, classify_type, item_usage,
                        sort_id, price, max_count, max_usage_count, stack_num,
                        is_consumable, is_gift, is_furniture, is_material,
                        is_exp_card, is_character,
                        source, version, raw_data, updated_at
                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                ''', (
                    safe_get(item, 'id'),
                    safe_get(item, 'name'),
                    safe_get(item.item_type, 'value', str(item.item_type)) if hasattr(item, 'item_type') else 'NONE',
                    safe_get(item, 'description'),
                    safe_int(item, 'rarity'),
                    safe_get(item, 'icon_id'),
                    safe_get(item, 'usage'),
                    safe_get(item, 'obtain_approach'),
                    safe_get(item, 'classify_type'),
                    safe_get(item, 'item_usage'),
                    safe_int(item, 'sort_id'),
                    safe_int(item, 'price'),
                    safe_int(item, 'max_count'),
                    safe_int(item, 'max_usage_count'),
                    safe_int(item, 'stack_num'),
                    safe_bool(item, 'is_consumable'),
                    safe_bool(item, 'is_gift'),
                    safe_bool(item, 'is_furniture'),
                    safe_bool(item, 'is_material'),
                    safe_bool(item, 'is_exp_card'),
                    safe_bool(item, 'is_character'),
                    item.source.name if hasattr(item.source, 'name') else str(item.source),
                    item.version.version if item.version else '',
                    json.dumps(item.to_dict(), ensure_ascii=False),
                    datetime.now().isoformat()
                ))

                # 删除旧的配方
                cursor.execute('''
                    DELETE FROM recipe_materials WHERE recipe_id IN (
                        SELECT id FROM item_recipes WHERE item_id = ?
                    )
                ''', (item.id,))
                cursor.execute('DELETE FROM item_recipes WHERE item_id = ?', (item.id,))

                # 保存配方
                recipe = getattr(item, 'recipe', None)
                if recipe:
                    cursor.execute('''
                        INSERT INTO item_recipes (item_id, cost_gold)
                        VALUES (?, ?)
                    ''', (item.id, recipe.cost_gold))
                    recipe_id = cursor.lastrowid

                    # 保存材料
                    for material in recipe.materials:
                        cursor.execute('''
                            INSERT INTO recipe_materials (recipe_id, item_id, item_name, count)
                            VALUES (?, ?, ?, ?)
                        ''', (recipe_id, material.item_id, material.item_name, material.count))

            return True

        except Exception as e:
            logger.error(f"保存物品 {item.id} 失败: {e}")
            return False

    def get_item(self, item_id: str) -> Optional[Item]:
        """获取物品数据"""
        try:
            conn = self._get_connection()
            cursor = conn.cursor()

            cursor.execute('SELECT raw_data FROM items WHERE id = ?', (item_id,))
            row = cursor.fetchone()
            if not row:
                return None

            raw_data = json.loads(row['raw_data'])
            return Item.from_dict(raw_data)

        except Exception as e:
            logger.error(f"获取物品 {item_id} 失败: {e}")
            return None

    def query_items(
        self,
        item_type: Optional[Union[ItemType, str]] = None,
        rarity: Optional[int] = None,
        min_rarity: Optional[int] = None,
        max_rarity: Optional[int] = None,
        is_material: Optional[bool] = None,
        is_exp_card: Optional[bool] = None,
        limit: int = 100,
        offset: int = 0
    ) -> List[Dict[str, Any]]:
        """
        查询物品

        Args:
            item_type: 物品类型(枚举或字符串)
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
        try:
            conn = self._get_connection()
            cursor = conn.cursor()

            conditions = []
            params = []

            if item_type:
                conditions.append("item_type = ?")
                it_value = item_type.value if isinstance(item_type, ItemType) else str(item_type)
                params.append(it_value)
            if rarity is not None:
                conditions.append("rarity = ?")
                params.append(rarity)
            if min_rarity is not None:
                conditions.append("rarity >= ?")
                params.append(min_rarity)
            if max_rarity is not None:
                conditions.append("rarity <= ?")
                params.append(max_rarity)
            if is_material is not None:
                conditions.append("is_material = ?")
                params.append(1 if is_material else 0)
            if is_exp_card is not None:
                conditions.append("is_exp_card = ?")
                params.append(1 if is_exp_card else 0)

            where_clause = " AND ".join(conditions) if conditions else "1=1"

            cursor.execute(f'''
                SELECT id, name, item_type, rarity, icon_id, is_material, is_exp_card, updated_at
                FROM items
                WHERE {where_clause}
                ORDER BY rarity DESC, sort_id, name
                LIMIT ? OFFSET ?
            ''', (*params, limit, offset))

            return [dict(row) for row in cursor.fetchall()]

        except Exception as e:
            logger.error(f"查询物品失败: {e}")
            return []

    def get_item_recipe(self, item_id: str) -> Optional[Dict[str, Any]]:
        """
        获取物品合成配方

        Args:
            item_id: 物品ID

        Returns:
            配方信息
        """
        try:
            conn = self._get_connection()
            cursor = conn.cursor()

            cursor.execute('''
                SELECT ir.id, ir.cost_gold, i.name, i.rarity
                FROM item_recipes ir
                JOIN items i ON ir.item_id = i.id
                WHERE ir.item_id = ?
            ''', (item_id,))

            recipe_row = cursor.fetchone()
            if not recipe_row:
                return None

            cursor.execute('''
                SELECT rm.item_id, rm.item_name, rm.count, i.rarity, i.icon_id
                FROM recipe_materials rm
                LEFT JOIN items i ON rm.item_id = i.id
                WHERE rm.recipe_id = ?
            ''', (recipe_row['id'],))

            materials = [dict(row) for row in cursor.fetchall()]

            return {
                'item_id': item_id,
                'item_name': recipe_row['name'],
                'rarity': recipe_row['rarity'],
                'cost_gold': recipe_row['cost_gold'],
                'materials': materials
            }

        except Exception as e:
            logger.error(f"获取物品配方失败: {e}")
            return None

    def get_material_tree(self, item_id: str, depth: int = 3) -> Dict[str, Any]:
        """
        获取材料合成树

        Args:
            item_id: 物品ID
            depth: 递归深度

        Returns:
            材料树
        """
        try:
            conn = self._get_connection()
            cursor = conn.cursor()

            cursor.execute('''
                SELECT id, name, rarity, item_type, is_material
                FROM items WHERE id = ?
            ''', (item_id,))

            item_row = cursor.fetchone()
            if not item_row:
                return {'item_id': item_id, 'error': 'Item not found'}

            result = {
                'item_id': item_id,
                'name': item_row['name'],
                'rarity': item_row['rarity'],
                'is_material': item_row['is_material'],
                'materials': []
            }

            if depth <= 0:
                result['leaf'] = True
                return result

            # 获取配方
            cursor.execute('''
                SELECT ir.id, ir.cost_gold
                FROM item_recipes ir
                WHERE ir.item_id = ?
            ''', (item_id,))

            recipe_row = cursor.fetchone()
            if recipe_row:
                result['cost_gold'] = recipe_row['cost_gold']

                cursor.execute('''
                    SELECT rm.item_id, rm.count
                    FROM recipe_materials rm
                    WHERE rm.recipe_id = ?
                ''', (recipe_row['id'],))

                for mat_row in cursor.fetchall():
                    mat_tree = self.get_material_tree(mat_row['item_id'], depth - 1)
                    mat_tree['count'] = mat_row['count']
                    result['materials'].append(mat_tree)

            return result

        except Exception as e:
            logger.error(f"获取材料树失败: {e}")
            return {'item_id': item_id, 'error': str(e)}

    # ==================== 敌人操作 ====================

    def save_enemy(self, enemy: Enemy) -> bool:
        """
        保存敌人数据

        Args:
            enemy: 敌人对象

        Returns:
            是否保存成功
        """
        try:
            with self._transaction() as conn:
                cursor = conn.cursor()

                # 保存主表
                cursor.execute('''
                    INSERT OR REPLACE INTO enemies (
                        id, name, enemy_level, description,
                        max_hp, atk, def, magic_resistance,
                        move_speed, attack_speed, base_attack_time,
                        hp_recovery_per_sec, sp_recovery_per_sec,
                        mass_level, taunt_level,
                        ep_damage_resistance, ep_resistance,
                        stun_immune, silence_immune, sleep_immune, frozen_immune,
                        levitate_immune, disarmed_combat_immune, feared_immune,
                        icon_id, sort_id,
                        source, version, raw_data, updated_at
                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                ''', (
                    enemy.id,
                    enemy.name,
                    enemy.enemy_level.value if hasattr(enemy.enemy_level, 'value') else str(enemy.enemy_level),
                    enemy.description,
                    enemy.max_hp,
                    enemy.atk,
                    enemy.def_,
                    enemy.magic_resistance,
                    enemy.move_speed,
                    enemy.attack_speed,
                    enemy.base_attack_time,
                    enemy.hp_recovery_per_sec,
                    getattr(enemy, 'sp_recovery_per_sec', 0.0),
                    enemy.mass_level,
                    enemy.taunt_level,
                    getattr(enemy, 'ep_damage_resistance', 0.0),
                    getattr(enemy, 'ep_resistance', 0.0),
                    enemy.stun_immune,
                    enemy.silence_immune,
                    enemy.sleep_immune,
                    enemy.frozen_immune,
                    getattr(enemy, 'levitate_immune', False),
                    getattr(enemy, 'disarmed_combat_immune', False),
                    getattr(enemy, 'feared_immune', False),
                    enemy.icon_id,
                    getattr(enemy, 'sort_id', 0),
                    enemy.source.name if hasattr(enemy.source, 'name') else str(enemy.source),
                    enemy.version.version if enemy.version else '',
                    json.dumps(enemy.to_dict(), ensure_ascii=False),
                    datetime.now().isoformat()
                ))

                # 删除旧的关联数据
                cursor.execute('DELETE FROM enemy_abilities WHERE enemy_id = ?', (enemy.id,))
                cursor.execute('DELETE FROM enemy_attacks WHERE enemy_id = ?', (enemy.id,))

                # 保存能力
                for ability in enemy.abilities:
                    cursor.execute('''
                        INSERT INTO enemy_abilities (
                            enemy_id, ability_id, ability_name, description, icon_id
                        ) VALUES (?, ?, ?, ?, ?)
                    ''', (
                        enemy.id,
                        ability.ability_id,
                        ability.ability_name,
                        ability.description,
                        ability.icon_id
                    ))

                # 保存攻击模式
                for attack in getattr(enemy, 'attacks', []):
                    cursor.execute('''
                        INSERT INTO enemy_attacks (
                            enemy_id, attack_type, damage_type, attack_range,
                            attack_speed, attack_times
                        ) VALUES (?, ?, ?, ?, ?, ?)
                    ''', (
                        enemy.id,
                        getattr(attack, 'attack_type', ''),
                        getattr(attack, 'damage_type', ''),
                        getattr(attack, 'attack_range', 0.0),
                        getattr(attack, 'attack_speed', 100.0),
                        getattr(attack, 'attack_times', 1)
                    ))

            return True

        except Exception as e:
            logger.error(f"保存敌人 {enemy.id} 失败: {e}")
            return False

    def get_enemy(self, enemy_id: str) -> Optional[Enemy]:
        """获取敌人数据"""
        try:
            conn = self._get_connection()
            cursor = conn.cursor()

            cursor.execute('SELECT raw_data FROM enemies WHERE id = ?', (enemy_id,))
            row = cursor.fetchone()
            if not row:
                return None

            raw_data = json.loads(row['raw_data'])
            return Enemy.from_dict(raw_data)

        except Exception as e:
            logger.error(f"获取敌人 {enemy_id} 失败: {e}")
            return None

    def query_enemies(
        self,
        enemy_level: Optional[Union[EnemyLevel, str]] = None,
        min_hp: Optional[int] = None,
        max_hp: Optional[int] = None,
        min_atk: Optional[int] = None,
        max_atk: Optional[int] = None,
        has_ability: Optional[bool] = None,
        limit: int = 100,
        offset: int = 0
    ) -> List[Dict[str, Any]]:
        """
        查询敌人

        Args:
            enemy_level: 敌人等级(枚举或字符串)
            min_hp: 最小生命值
            max_hp: 最大生命值
            min_atk: 最小攻击力
            max_atk: 最大攻击力
            has_ability: 是否有特殊能力
            limit: 返回数量限制
            offset: 偏移量

        Returns:
            敌人列表
        """
        try:
            conn = self._get_connection()
            cursor = conn.cursor()

            conditions = []
            params = []

            if enemy_level:
                conditions.append("enemy_level = ?")
                el_value = enemy_level.value if isinstance(enemy_level, EnemyLevel) else str(enemy_level)
                params.append(el_value)
            if min_hp is not None:
                conditions.append("max_hp >= ?")
                params.append(min_hp)
            if max_hp is not None:
                conditions.append("max_hp <= ?")
                params.append(max_hp)
            if min_atk is not None:
                conditions.append("atk >= ?")
                params.append(min_atk)
            if max_atk is not None:
                conditions.append("atk <= ?")
                params.append(max_atk)

            where_clause = " AND ".join(conditions) if conditions else "1=1"

            cursor.execute(f'''
                SELECT id, name, enemy_level, max_hp, atk, def, magic_resistance,
                       move_speed, icon_id, updated_at
                FROM enemies
                WHERE {where_clause}
                ORDER BY enemy_level DESC, sort_id, name
                LIMIT ? OFFSET ?
            ''', (*params, limit, offset))

            return [dict(row) for row in cursor.fetchall()]

        except Exception as e:
            logger.error(f"查询敌人失败: {e}")
            return []

    def get_enemy_abilities(self, enemy_id: str) -> List[Dict[str, Any]]:
        """获取敌人能力列表"""
        try:
            conn = self._get_connection()
            cursor = conn.cursor()

            cursor.execute('''
                SELECT * FROM enemy_abilities
                WHERE enemy_id = ?
            ''', (enemy_id,))

            return [dict(row) for row in cursor.fetchall()]

        except Exception as e:
            logger.error(f"获取敌人能力失败: {e}")
            return []

    def compare_enemies(self, enemy_ids: List[str]) -> List[Dict[str, Any]]:
        """
        对比多个敌人的属性

        Args:
            enemy_ids: 敌人ID列表

        Returns:
            对比数据
        """
        try:
            conn = self._get_connection()
            cursor = conn.cursor()

            placeholders = ','.join('?' * len(enemy_ids))
            cursor.execute(f'''
                SELECT id, name, enemy_level, max_hp, atk, def, magic_resistance,
                       move_speed, attack_speed, mass_level
                FROM enemies
                WHERE id IN ({placeholders})
            ''', enemy_ids)

            return [dict(row) for row in cursor.fetchall()]

        except Exception as e:
            logger.error(f"对比敌人失败: {e}")
            return []

    # ==================== 统计和批量操作 ====================

    def get_statistics(self) -> Dict[str, Any]:
        """
        获取数据统计

        Returns:
            统计信息
        """
        try:
            conn = self._get_connection()
            cursor = conn.cursor()

            stats = {}

            # 各表数量
            for table in ['operators', 'stages', 'items', 'enemies']:
                cursor.execute(f'SELECT COUNT(*) FROM {table}')
                stats[f'{table}_count'] = cursor.fetchone()[0]

            for table in ['item_recipes', 'recipe_materials']:
                cursor.execute(f'SELECT COUNT(*) FROM {table}')
                stats[f'{table}_count'] = cursor.fetchone()[0]

            # 干员职业分布
            cursor.execute('''
                SELECT profession, COUNT(*) as count
                FROM operators
                GROUP BY profession
                ORDER BY count DESC
            ''')
            stats['operator_professions'] = [dict(row) for row in cursor.fetchall()]

            # 干员稀有度分布
            cursor.execute('''
                SELECT rarity, COUNT(*) as count
                FROM operators
                GROUP BY rarity
                ORDER BY rarity DESC
            ''')
            stats['operator_rarities'] = [dict(row) for row in cursor.fetchall()]

            # 关卡类型分布
            cursor.execute('''
                SELECT stage_type, COUNT(*) as count
                FROM stages
                GROUP BY stage_type
                ORDER BY count DESC
            ''')
            stats['stage_types'] = [dict(row) for row in cursor.fetchall()]

            # 物品类型分布
            cursor.execute('''
                SELECT item_type, COUNT(*) as count
                FROM items
                GROUP BY item_type
                ORDER BY count DESC
            ''')
            stats['item_types'] = [dict(row) for row in cursor.fetchall()]

            # 敌人等级分布
            cursor.execute('''
                SELECT enemy_level, COUNT(*) as count
                FROM enemies
                GROUP BY enemy_level
                ORDER BY count DESC
            ''')
            stats['enemy_levels'] = [dict(row) for row in cursor.fetchall()]

            return stats

        except Exception as e:
            logger.error(f"获取统计失败: {e}")
            return {}

    def save_version_info(self, source: str, version: str, commit_hash: str = '') -> bool:
        """保存版本信息"""
        try:
            with self._transaction() as conn:
                cursor = conn.cursor()
                cursor.execute('''
                    INSERT OR REPLACE INTO version_info (source, version, commit_hash, updated_at)
                    VALUES (?, ?, ?, ?)
                ''', (source, version, commit_hash, datetime.now().isoformat()))
            return True
        except Exception as e:
            logger.error(f"保存版本信息失败: {e}")
            return False

    def get_version_info(self, source: str) -> Optional[Dict[str, Any]]:
        """获取版本信息"""
        try:
            conn = self._get_connection()
            cursor = conn.cursor()
            cursor.execute('SELECT * FROM version_info WHERE source = ?', (source,))
            row = cursor.fetchone()
            return dict(row) if row else None
        except Exception as e:
            logger.error(f"获取版本信息失败: {e}")
            return None

    def clear_all_data(self) -> bool:
        """清空所有数据"""
        try:
            with self._transaction() as conn:
                cursor = conn.cursor()
                for table in ['operators', 'stages', 'items', 'enemies', 'version_info']:
                    cursor.execute(f'DELETE FROM {table}')
            logger.info("数据库已清空")
            return True
        except Exception as e:
            logger.error(f"清空数据失败: {e}")
            return False

    def vacuum(self) -> bool:
        """优化数据库"""
        try:
            conn = self._get_connection()
            conn.execute('VACUUM')
            logger.info("数据库优化完成")
            return True
        except Exception as e:
            logger.error(f"数据库优化失败: {e}")
            return False
