# ==========================================================================
# Copyright (C) 2026 Ethernos Studio
# This file is part of Arknights Auto Machine (AAM).
#
# AAM is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# AAM is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with AAM. If not, see <https://www.gnu.org/licenses/>.
# ==========================================================================
"""
@file level_analyzer.py
@author dhjs0000
@brief 关卡数据分析器 - 生产级完整实现

版本: v1.0.0
功能: 解析明日方舟关卡数据，包括地图布局、敌人路径、波次信息等
算法: JSON 解析 → 图构建 → 路径分析 → 波次预测
性能: O(V + E) 图遍历，O(W * E) 波次分析，其中 V=顶点数, E=边数, W=波次数
"""

import json
import heapq
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple, Set, Any
from enum import Enum, auto
from pathlib import Path
import logging

# 配置日志
logger = logging.getLogger(__name__)


class TileType(Enum):
    """地块类型枚举"""
    NONE = 0          # 不可通行
    FLOOR = 1         # 地板（可部署）
    WALL = 2          # 墙体（不可通行）
    START = 3         # 起点（敌人出生点）
    END = 4           # 终点（保护目标）
    FLYING_START = 5  # 飞行敌人起点
    FLYING_END = 6    # 飞行敌人终点
    HOLE = 7          # 地穴（可掉落）
    HEALING = 8       # 治疗符文
    DEFENSE = 9       # 防御符文
    SPEED = 10        # 加速符文


class EnemyType(Enum):
    """敌人类型枚举"""
    NORMAL = auto()      # 普通敌人
    ELITE = auto()       # 精英敌人
    BOSS = auto()        # Boss
    FLYING = auto()      # 飞行敌人
    INVISIBLE = auto()   # 隐形敌人


@dataclass
class Position:
    """二维坐标位置"""
    row: int
    col: int
    
    def __hash__(self) -> int:
        return hash((self.row, self.col))
    
    def __eq__(self, other: object) -> bool:
        if not isinstance(other, Position):
            return NotImplemented
        return self.row == other.row and self.col == other.col
    
    def __add__(self, other: 'Position') -> 'Position':
        return Position(self.row + other.row, self.col + other.col)
    
    def __sub__(self, other: 'Position') -> 'Position':
        return Position(self.row - other.row, self.col - other.col)
    
    def manhattan_distance(self, other: 'Position') -> int:
        """计算曼哈顿距离"""
        return abs(self.row - other.row) + abs(self.col - other.col)
    
    def euclidean_distance(self, other: 'Position') -> float:
        """计算欧几里得距离"""
        return ((self.row - other.row) ** 2 + (self.col - other.col) ** 2) ** 0.5


@dataclass
class Tile:
    """地图地块信息"""
    position: Position
    tile_type: TileType
    height: float = 0.0
    buildable_type: int = 0  # 0=不可部署, 1=近战位, 2=远程位, 3=均可
    
    def can_deploy_melee(self) -> bool:
        """是否可以部署近战干员"""
        return self.buildable_type in (1, 3) and self.tile_type == TileType.FLOOR
    
    def can_deploy_ranged(self) -> bool:
        """是否可以部署远程干员"""
        return self.buildable_type in (2, 3) and self.tile_type == TileType.FLOOR
    
    def is_passable(self, flying: bool = False) -> bool:
        """是否可通行"""
        if flying:
            return self.tile_type in (TileType.FLOOR, TileType.START, TileType.END,
                                     TileType.FLYING_START, TileType.FLYING_END, TileType.HOLE)
        return self.tile_type in (TileType.FLOOR, TileType.START, TileType.END, TileType.HOLE)


@dataclass
class RoutePoint:
    """路径点"""
    position: Position
    delay: float = 0.0  # 到达此点的延迟（秒）
    speed_modifier: float = 1.0  # 速度修正


@dataclass
class EnemyRoute:
    """敌人路径信息"""
    route_id: str
    points: List[RoutePoint] = field(default_factory=list)
    loop: bool = False  # 是否循环路径
    
    def get_total_distance(self) -> float:
        """计算路径总长度"""
        if len(self.points) < 2:
            return 0.0
        
        total = 0.0
        for i in range(len(self.points) - 1):
            total += self.points[i].position.euclidean_distance(self.points[i + 1].position)
        return total
    
    def get_position_at_time(self, time: float, speed: float) -> Optional[Position]:
        """获取指定时间点的位置（线性插值）"""
        if not self.points:
            return None
        
        current_time = 0.0
        for i in range(len(self.points) - 1):
            p1, p2 = self.points[i], self.points[i + 1]
            distance = p1.position.euclidean_distance(p2.position)
            segment_time = distance / (speed * p1.speed_modifier)
            
            if current_time + segment_time >= time:
                # 在此段内
                t = (time - current_time) / segment_time
                return Position(
                    int(p1.position.row + (p2.position.row - p1.position.row) * t),
                    int(p1.position.col + (p2.position.col - p1.position.col) * t)
                )
            
            current_time += segment_time
        
        # 到达终点
        return self.points[-1].position if self.points else None


@dataclass
class EnemySpawn:
    """敌人生成信息"""
    enemy_id: str
    count: int
    interval: float  # 生成间隔（秒）
    route_id: str
    delay: float = 0.0  # 首次生成延迟


@dataclass
class Wave:
    """波次信息"""
    wave_id: str
    spawns: List[EnemySpawn] = field(default_factory=list)
    pre_delay: float = 0.0  # 波次开始前延迟
    post_delay: float = 0.0  # 波次结束后延迟
    
    def get_total_enemies(self) -> int:
        """获取波次总敌人数"""
        return sum(spawn.count for spawn in self.spawns)
    
    def get_duration(self) -> float:
        """获取波次持续时间"""
        max_duration = 0.0
        for spawn in self.spawns:
            duration = spawn.delay + (spawn.count - 1) * spawn.interval
            max_duration = max(max_duration, duration)
        return max_duration + self.pre_delay + self.post_delay


@dataclass
class LevelData:
    """关卡数据"""
    level_id: str
    level_name: str = ""
    difficulty: int = 1
    
    # 地图信息
    map_width: int = 0
    map_height: int = 0
    tiles: List[Tile] = field(default_factory=list)
    
    # 路径信息
    routes: Dict[str, EnemyRoute] = field(default_factory=dict)
    
    # 波次信息
    waves: List[Wave] = field(default_factory=list)
    
    # 特殊机制
    max_deploy_count: int = 8
    initial_cost: int = 10
    max_cost: int = 99
    cost_recovery: float = 1.0  # 每秒自然回费
    
    def get_tile(self, row: int, col: int) -> Optional[Tile]:
        """获取指定位置的地图地块"""
        for tile in self.tiles:
            if tile.position.row == row and tile.position.col == col:
                return tile
        return None
    
    def get_tiles_by_type(self, tile_type: TileType) -> List[Tile]:
        """获取指定类型的所有地块"""
        return [tile for tile in self.tiles if tile.tile_type == tile_type]
    
    def get_deployable_positions(self, melee: bool = True, ranged: bool = True) -> List[Position]:
        """获取可部署位置列表"""
        positions = []
        for tile in self.tiles:
            if melee and tile.can_deploy_melee():
                positions.append(tile.position)
            elif ranged and tile.can_deploy_ranged():
                positions.append(tile.position)
        return positions
    
    def get_route(self, route_id: str) -> Optional[EnemyRoute]:
        """获取指定路径"""
        return self.routes.get(route_id)
    
    def get_wave(self, wave_index: int) -> Optional[Wave]:
        """获取指定波次"""
        if 0 <= wave_index < len(self.waves):
            return self.waves[wave_index]
        return None
    
    def get_total_enemies(self) -> int:
        """获取关卡总敌人数"""
        return sum(wave.get_total_enemies() for wave in self.waves)
    
    def get_estimated_duration(self) -> float:
        """获取预计关卡时长"""
        total = 0.0
        for wave in self.waves:
            total += wave.get_duration()
        return total


class PathFinder:
    """路径查找器 - A* 算法实现"""
    
    def __init__(self, level_data: LevelData):
        self.level = level_data
        self._cache: Dict[Tuple[Position, Position], List[Position]] = {}
    
    def find_path(self, start: Position, end: Position, 
                  flying: bool = False) -> List[Position]:
        """
        使用 A* 算法查找最短路径
        
        Args:
            start: 起点
            end: 终点
            flying: 是否飞行路径
            
        Returns:
            路径点列表，失败返回空列表
            
        Time Complexity: O((V + E) log V)
        Space Complexity: O(V)
        """
        # 检查缓存
        cache_key = (start, end, flying)
        if cache_key in self._cache:
            return self._cache[cache_key]
        
        # A* 算法
        open_set: List[Tuple[float, int, Position]] = [(0, 0, start)]
        heapq.heapify(open_set)
        
        came_from: Dict[Position, Position] = {}
        g_score: Dict[Position, float] = {start: 0}
        f_score: Dict[Position, float] = {start: start.manhattan_distance(end)}
        
        counter = 0
        closed_set: Set[Position] = set()
        
        while open_set:
            _, _, current = heapq.heappop(open_set)
            
            if current in closed_set:
                continue
            
            if current == end:
                # 重建路径
                path = self._reconstruct_path(came_from, current)
                self._cache[cache_key] = path
                return path
            
            closed_set.add(current)
            
            for neighbor in self._get_neighbors(current, flying):
                if neighbor in closed_set:
                    continue
                
                tentative_g = g_score[current] + 1
                
                if neighbor not in g_score or tentative_g < g_score[neighbor]:
                    came_from[neighbor] = current
                    g_score[neighbor] = tentative_g
                    f_score[neighbor] = tentative_g + neighbor.manhattan_distance(end)
                    counter += 1
                    heapq.heappush(open_set, (f_score[neighbor], counter, neighbor))
        
        return []  # 无路径
    
    def _get_neighbors(self, pos: Position, flying: bool) -> List[Position]:
        """获取相邻可通行位置"""
        neighbors = []
        directions = [
            Position(0, 1),   # 右
            Position(0, -1),  # 左
            Position(1, 0),   # 下
            Position(-1, 0),  # 上
        ]
        
        for direction in directions:
            neighbor = pos + direction
            tile = self.level.get_tile(neighbor.row, neighbor.col)
            if tile and tile.is_passable(flying):
                neighbors.append(neighbor)
        
        return neighbors
    
    def _reconstruct_path(self, came_from: Dict[Position, Position], 
                          current: Position) -> List[Position]:
        """重建路径"""
        path = [current]
        while current in came_from:
            current = came_from[current]
            path.append(current)
        path.reverse()
        return path
    
    def find_all_paths_to_end(self, flying: bool = False) -> Dict[Position, List[Position]]:
        """
        查找所有起点到终点的路径
        
        Returns:
            起点到路径的映射
        """
        paths = {}
        
        # 获取起点和终点
        if flying:
            starts = [tile.position for tile in self.level.get_tiles_by_type(TileType.FLYING_START)]
            ends = [tile.position for tile in self.level.get_tiles_by_type(TileType.FLYING_END)]
        else:
            starts = [tile.position for tile in self.level.get_tiles_by_type(TileType.START)]
            ends = [tile.position for tile in self.level.get_tiles_by_type(TileType.END)]
        
        for start in starts:
            for end in ends:
                path = self.find_path(start, end, flying)
                if path:
                    paths[start] = path
                    break
        
        return paths


class LevelAnalyzer:
    """关卡分析器"""
    
    def __init__(self, level_data: LevelData):
        self.level = level_data
        self.path_finder = PathFinder(level_data)
        self._analysis_cache: Dict[str, Any] = {}
    
    def analyze(self) -> Dict[str, Any]:
        """
        执行完整关卡分析
        
        Returns:
            分析结果字典
        """
        if 'full_analysis' in self._analysis_cache:
            return self._analysis_cache['full_analysis']
        
        analysis = {
            'level_id': self.level.level_id,
            'level_name': self.level.level_name,
            'difficulty': self.level.difficulty,
            
            # 地图分析
            'map_size': (self.level.map_width, self.level.map_height),
            'deployable_melee': len([t for t in self.level.tiles if t.can_deploy_melee()]),
            'deployable_ranged': len([t for t in self.level.tiles if t.can_deploy_ranged()]),
            
            # 路径分析
            'ground_routes': self._analyze_ground_routes(),
            'flying_routes': self._analyze_flying_routes(),
            
            # 波次分析
            'wave_analysis': self._analyze_waves(),
            
            # 战略要点
            'strategic_points': self._find_strategic_points(),
            
            # 预估数据
            'total_enemies': self.level.get_total_enemies(),
            'estimated_duration': self.level.get_estimated_duration(),
        }
        
        self._analysis_cache['full_analysis'] = analysis
        return analysis
    
    def _analyze_ground_routes(self) -> List[Dict[str, Any]]:
        """分析地面路径"""
        routes = []
        paths = self.path_finder.find_all_paths_to_end(flying=False)
        
        for start, path in paths.items():
            route_info = {
                'start': (start.row, start.col),
                'end': (path[-1].row, path[-1].col),
                'length': len(path),
                'key_points': self._find_route_key_points(path),
            }
            routes.append(route_info)
        
        return routes
    
    def _analyze_flying_routes(self) -> List[Dict[str, Any]]:
        """分析飞行路径"""
        routes = []
        paths = self.path_finder.find_all_paths_to_end(flying=True)
        
        for start, path in paths.items():
            route_info = {
                'start': (start.row, start.col),
                'end': (path[-1].row, path[-1].col),
                'length': len(path),
            }
            routes.append(route_info)
        
        return routes
    
    def _analyze_waves(self) -> List[Dict[str, Any]]:
        """分析波次"""
        wave_analysis = []
        
        for i, wave in enumerate(self.level.waves):
            wave_info = {
                'wave_number': i + 1,
                'wave_id': wave.wave_id,
                'total_enemies': wave.get_total_enemies(),
                'duration': wave.get_duration(),
                'enemy_types': self._get_wave_enemy_types(wave),
                'spawn_pattern': self._analyze_spawn_pattern(wave),
            }
            wave_analysis.append(wave_info)
        
        return wave_analysis
    
    def _get_wave_enemy_types(self, wave: Wave) -> Set[str]:
        """获取波次中的敌人类型"""
        enemy_types = set()
        for spawn in wave.spawns:
            enemy_types.add(spawn.enemy_id)
        return enemy_types
    
    def _analyze_spawn_pattern(self, wave: Wave) -> Dict[str, Any]:
        """分析生成模式"""
        if not wave.spawns:
            return {}
        
        # 按时间排序的生成事件
        spawn_events = []
        for spawn in wave.spawns:
            for i in range(spawn.count):
                time = spawn.delay + i * spawn.interval
                spawn_events.append((time, spawn.enemy_id, spawn.route_id))
        
        spawn_events.sort(key=lambda x: x[0])
        
        return {
            'total_spawns': len(spawn_events),
            'first_spawn_time': spawn_events[0][0] if spawn_events else 0,
            'last_spawn_time': spawn_events[-1][0] if spawn_events else 0,
            'peak_spawn_time': self._find_peak_spawn_time(spawn_events),
        }
    
    def _find_peak_spawn_time(self, spawn_events: List[Tuple[float, str, str]]) -> float:
        """找到生成最密集的时间点"""
        if len(spawn_events) < 2:
            return 0.0
        
        window_size = 5  # 5秒窗口
        max_count = 0
        peak_time = 0.0
        
        for i, (time, _, _) in enumerate(spawn_events):
            count = sum(1 for t, _, _ in spawn_events 
                       if time <= t < time + window_size)
            if count > max_count:
                max_count = count
                peak_time = time
        
        return peak_time
    
    def _find_strategic_points(self) -> List[Dict[str, Any]]:
        """查找战略要点（路径交汇点、狭窄通道等）"""
        points = []
        
        # 获取所有路径
        all_paths = []
        all_paths.extend(self.path_finder.find_all_paths_to_end(flying=False).values())
        all_paths.extend(self.path_finder.find_all_paths_to_end(flying=True).values())
        
        if not all_paths:
            return points
        
        # 统计每个位置被多少路径经过
        position_count: Dict[Position, int] = {}
        for path in all_paths:
            for pos in path:
                position_count[pos] = position_count.get(pos, 0) + 1
        
        # 找出交汇点（被多条路径经过）
        for pos, count in position_count.items():
            if count > 1:
                tile = self.level.get_tile(pos.row, pos.col)
                if tile and (tile.can_deploy_melee() or tile.can_deploy_ranged()):
                    points.append({
                        'position': (pos.row, pos.col),
                        'type': 'intersection',
                        'path_count': count,
                        'can_deploy_melee': tile.can_deploy_melee(),
                        'can_deploy_ranged': tile.can_deploy_ranged(),
                    })
        
        # 按路径经过次数排序
        points.sort(key=lambda x: x['path_count'], reverse=True)
        
        return points[:10]  # 返回前10个战略要点
    
    def _find_route_key_points(self, path: List[Position]) -> List[Dict[str, Any]]:
        """查找路径关键点（转弯点）"""
        if len(path) < 3:
            return []
        
        key_points = []
        
        for i in range(1, len(path) - 1):
            prev_dir = (path[i].row - path[i-1].row, path[i].col - path[i-1].col)
            next_dir = (path[i+1].row - path[i].row, path[i+1].col - path[i].col)
            
            if prev_dir != next_dir:
                # 方向改变，是关键点
                tile = self.level.get_tile(path[i].row, path[i].col)
                key_points.append({
                    'position': (path[i].row, path[i].col),
                    'index': i,
                    'can_deploy': tile.can_deploy_melee() if tile else False,
                })
        
        return key_points
    
    def predict_enemy_positions(self, elapsed_time: float) -> Dict[str, List[Position]]:
        """
        预测指定时间点的敌人位置
        
        Args:
            elapsed_time: 已流逝的时间（秒）
            
        Returns:
            路径ID到位置列表的映射
        """
        positions: Dict[str, List[Position]] = {}
        
        current_time = 0.0
        for wave in self.level.waves:
            current_time += wave.pre_delay
            
            for spawn in wave.spawns:
                route = self.level.get_route(spawn.route_id)
                if not route:
                    continue
                
                for i in range(spawn.count):
                    spawn_time = current_time + spawn.delay + i * spawn.interval
                    if spawn_time <= elapsed_time:
                        # 这个敌人已经生成
                        travel_time = elapsed_time - spawn_time
                        pos = route.get_position_at_time(travel_time, 1.0)  # 假设速度为1
                        if pos:
                            if spawn.route_id not in positions:
                                positions[spawn.route_id] = []
                            positions[spawn.route_id].append(pos)
            
            current_time += wave.get_duration() + wave.post_delay
            
            if current_time > elapsed_time:
                break
        
        return positions
    
    def get_optimal_defense_positions(self, top_n: int = 5) -> List[Dict[str, Any]]:
        """
        获取最优防守位置
        
        Args:
            top_n: 返回前N个位置
            
        Returns:
            位置信息列表
        """
        strategic_points = self._find_strategic_points()
        
        # 评分
        scored_positions = []
        for point in strategic_points:
            score = point['path_count'] * 10
            
            if point['can_deploy_melee']:
                score += 5
            if point['can_deploy_ranged']:
                score += 3
            
            scored_positions.append({
                **point,
                'score': score,
            })
        
        # 按分数排序
        scored_positions.sort(key=lambda x: x['score'], reverse=True)
        
        return scored_positions[:top_n]


class LevelDataLoader:
    """关卡数据加载器"""
    
    @staticmethod
    def load_from_json(file_path: str) -> LevelData:
        """
        从JSON文件加载关卡数据
        
        Args:
            file_path: JSON文件路径
            
        Returns:
            LevelData对象
            
        Raises:
            FileNotFoundError: 文件不存在
            json.JSONDecodeError: JSON解析错误
            KeyError: 缺少必要字段
        """
        path = Path(file_path)
        if not path.exists():
            raise FileNotFoundError(f"Level data file not found: {file_path}")
        
        with open(path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        return LevelDataLoader._parse_level_data(data)
    
    @staticmethod
    def load_from_dict(data: Dict[str, Any]) -> LevelData:
        """从字典加载关卡数据"""
        return LevelDataLoader._parse_level_data(data)
    
    @staticmethod
    def _parse_level_data(data: Dict[str, Any]) -> LevelData:
        """解析关卡数据"""
        level = LevelData(
            level_id=data['levelId'],
            level_name=data.get('name', ''),
            difficulty=data.get('difficulty', 1),
            map_width=data.get('mapWidth', 0),
            map_height=data.get('mapHeight', 0),
            max_deploy_count=data.get('maxDeployCount', 8),
            initial_cost=data.get('initialCost', 10),
            max_cost=data.get('maxCost', 99),
            cost_recovery=data.get('costRecovery', 1.0),
        )
        
        # 解析地图
        if 'tiles' in data:
            level.tiles = LevelDataLoader._parse_tiles(data['tiles'])
        
        # 解析路径
        if 'routes' in data:
            level.routes = LevelDataLoader._parse_routes(data['routes'])
        
        # 解析波次
        if 'waves' in data:
            level.waves = LevelDataLoader._parse_waves(data['waves'])
        
        return level
    
    @staticmethod
    def _parse_tiles(tiles_data: List[Dict[str, Any]]) -> List[Tile]:
        """解析地图地块"""
        tiles = []
        
        for tile_data in tiles_data:
            tile = Tile(
                position=Position(
                    row=tile_data.get('row', 0),
                    col=tile_data.get('col', 0)
                ),
                tile_type=TileType(tile_data.get('tileType', 0)),
                height=tile_data.get('height', 0.0),
                buildable_type=tile_data.get('buildableType', 0),
            )
            tiles.append(tile)
        
        return tiles
    
    @staticmethod
    def _parse_routes(routes_data: List[Dict[str, Any]]) -> Dict[str, EnemyRoute]:
        """解析敌人路径"""
        routes = {}
        
        for route_data in routes_data:
            route = EnemyRoute(
                route_id=route_data['id'],
                loop=route_data.get('loop', False),
            )
            
            if 'points' in route_data:
                for point_data in route_data['points']:
                    point = RoutePoint(
                        position=Position(
                            row=point_data.get('row', 0),
                            col=point_data.get('col', 0)
                        ),
                        delay=point_data.get('delay', 0.0),
                        speed_modifier=point_data.get('speedModifier', 1.0),
                    )
                    route.points.append(point)
            
            routes[route.route_id] = route
        
        return routes
    
    @staticmethod
    def _parse_waves(waves_data: List[Dict[str, Any]]) -> List[Wave]:
        """解析波次"""
        waves = []
        
        for wave_data in waves_data:
            wave = Wave(
                wave_id=wave_data['id'],
                pre_delay=wave_data.get('preDelay', 0.0),
                post_delay=wave_data.get('postDelay', 0.0),
            )
            
            if 'spawns' in wave_data:
                for spawn_data in wave_data['spawns']:
                    spawn = EnemySpawn(
                        enemy_id=spawn_data['enemyId'],
                        count=spawn_data.get('count', 1),
                        interval=spawn_data.get('interval', 1.0),
                        route_id=spawn_data['routeId'],
                        delay=spawn_data.get('delay', 0.0),
                    )
                    wave.spawns.append(spawn)
            
            waves.append(wave)
        
        return waves


def analyze_level(file_path: str) -> Dict[str, Any]:
    """
    便捷函数：分析关卡文件
    
    Args:
        file_path: 关卡JSON文件路径
        
    Returns:
        分析结果字典
    """
    level_data = LevelDataLoader.load_from_json(file_path)
    analyzer = LevelAnalyzer(level_data)
    return analyzer.analyze()


if __name__ == '__main__':
    # 示例用法
    logging.basicConfig(level=logging.INFO)
    
    # 创建示例关卡数据
    example_level = {
        'levelId': 'main_01-01',
        'name': '1-1 孤岛',
        'difficulty': 1,
        'mapWidth': 10,
        'mapHeight': 6,
        'maxDeployCount': 8,
        'initialCost': 10,
        'tiles': [
            {'row': 2, 'col': 0, 'tileType': 3},  # START
            {'row': 2, 'col': 9, 'tileType': 4},  # END
            {'row': 2, 'col': 1, 'tileType': 1, 'buildableType': 3},
            {'row': 2, 'col': 2, 'tileType': 1, 'buildableType': 3},
            {'row': 2, 'col': 3, 'tileType': 1, 'buildableType': 3},
            {'row': 2, 'col': 4, 'tileType': 1, 'buildableType': 3},
            {'row': 2, 'col': 5, 'tileType': 1, 'buildableType': 3},
            {'row': 2, 'col': 6, 'tileType': 1, 'buildableType': 3},
            {'row': 2, 'col': 7, 'tileType': 1, 'buildableType': 3},
            {'row': 2, 'col': 8, 'tileType': 1, 'buildableType': 3},
        ],
        'routes': [
            {
                'id': 'route_1',
                'points': [
                    {'row': 2, 'col': 0},
                    {'row': 2, 'col': 9},
                ],
            }
        ],
        'waves': [
            {
                'id': 'wave_1',
                'preDelay': 5.0,
                'spawns': [
                    {'enemyId': 'enemy_1001', 'count': 5, 'interval': 2.0, 'routeId': 'route_1'},
                ],
            }
        ],
    }
    
    # 加载并分析
    level = LevelDataLoader.load_from_dict(example_level)
    analyzer = LevelAnalyzer(level)
    analysis = analyzer.analyze()
    
    # 打印分析结果
    print(f"关卡: {analysis['level_name']}")
    print(f"难度: {analysis['difficulty']}")
    print(f"地图大小: {analysis['map_size']}")
    print(f"可部署近战位: {analysis['deployable_melee']}")
    print(f"可部署远程位: {analysis['deployable_ranged']}")
    print(f"总敌人数: {analysis['total_enemies']}")
    print(f"预计时长: {analysis['estimated_duration']:.1f}秒")
    print(f"战略要点数: {len(analysis['strategic_points'])}")
