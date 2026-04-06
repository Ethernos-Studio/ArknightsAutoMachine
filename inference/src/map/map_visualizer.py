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
@file map_visualizer.py
@author dhjs0000
@brief 地图可视化器 - 生产级完整实现

版本: v1.0.0
功能: 将关卡地图数据可视化为图像，支持多种渲染模式
算法: 网格渲染 → 图元绘制 → 路径标注 → 输出
性能: O(W * H) 渲染时间，其中 W=宽度, H=高度
"""

import io
import math
from dataclasses import dataclass
from typing import List, Dict, Optional, Tuple, Union, Callable
from enum import Enum, auto
from pathlib import Path
import logging

try:
    from PIL import Image, ImageDraw, ImageFont
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False

from level_analyzer import LevelData, TileType, Position, Tile, EnemyRoute

logger = logging.getLogger(__name__)


class RenderMode(Enum):
    """渲染模式"""
    GRID = auto()        # 网格模式
    MINIMAL = auto()     # 极简模式
    DETAILED = auto()    # 详细模式
    STRATEGIC = auto()   # 战略模式（标注要点）


@dataclass
class ColorScheme:
    """颜色方案"""
    background: Tuple[int, int, int] = (30, 30, 30)
    grid_line: Tuple[int, int, int] = (60, 60, 60)
    
    # 地块颜色
    floor: Tuple[int, int, int] = (80, 80, 80)
    wall: Tuple[int, int, int] = (40, 40, 40)
    start: Tuple[int, int, int] = (255, 100, 100)
    end: Tuple[int, int, int] = (100, 255, 100)
    flying_start: Tuple[int, int, int] = (255, 150, 100)
    flying_end: Tuple[int, int, int] = (150, 255, 100)
    hole: Tuple[int, int, int] = (50, 50, 50)
    healing: Tuple[int, int, int] = (100, 255, 200)
    defense: Tuple[int, int, int] = (100, 200, 255)
    speed: Tuple[int, int, int] = (255, 255, 100)
    
    # 部署位颜色
    deploy_melee: Tuple[int, int, int] = (200, 150, 100)
    deploy_ranged: Tuple[int, int, int] = (100, 150, 200)
    deploy_both: Tuple[int, int, int] = (150, 200, 150)
    
    # 路径颜色
    route_ground: Tuple[int, int, int] = (255, 200, 100)
    route_flying: Tuple[int, int, int] = (200, 100, 255)
    
    # 文字颜色
    text: Tuple[int, int, int] = (255, 255, 255)
    text_shadow: Tuple[int, int, int] = (0, 0, 0)


@dataclass
class RenderConfig:
    """渲染配置"""
    tile_size: int = 32
    padding: int = 20
    show_grid: bool = True
    show_coordinates: bool = True
    show_routes: bool = True
    show_deploy_zones: bool = True
    show_strategic_points: bool = True
    color_scheme: ColorScheme = None
    
    def __post_init__(self):
        if self.color_scheme is None:
            self.color_scheme = ColorScheme()


class MapVisualizer:
    """地图可视化器"""
    
    def __init__(self, level_data: LevelData):
        if not HAS_PIL:
            raise ImportError("PIL (Pillow) is required for map visualization")
        
        self.level = level_data
        self.config = RenderConfig()
        
    def set_config(self, config: RenderConfig) -> 'MapVisualizer':
        """设置渲染配置"""
        self.config = config
        return self
    
    def render(self, mode: RenderMode = RenderMode.DETAILED) -> Image.Image:
        """
        渲染地图
        
        Args:
            mode: 渲染模式
            
        Returns:
            渲染后的图像
            
        Time Complexity: O(W * H)
        Space Complexity: O(W * H)
        """
        # 计算图像尺寸
        width = self.level.map_width * self.config.tile_size + 2 * self.config.padding
        height = self.level.map_height * self.config.tile_size + 2 * self.config.padding
        
        # 创建图像
        image = Image.new('RGB', (width, height), self.config.color_scheme.background)
        draw = ImageDraw.Draw(image)
        
        # 渲染网格背景
        if self.config.show_grid:
            self._render_grid(draw, width, height)
        
        # 渲染地块
        self._render_tiles(draw, mode)
        
        # 渲染路径
        if self.config.show_routes:
            self._render_routes(draw)
        
        # 渲染战略要点
        if mode == RenderMode.STRATEGIC and self.config.show_strategic_points:
            self._render_strategic_points(draw)
        
        # 渲染坐标
        if self.config.show_coordinates:
            self._render_coordinates(draw)
        
        return image
    
    def render_with_overlay(self, 
                           overlay_data: Dict[Position, Tuple[int, int, int]],
                           alpha: float = 0.5) -> Image.Image:
        """
        渲染带覆盖层的地图
        
        Args:
            overlay_data: 位置到颜色的映射
            alpha: 覆盖层透明度
            
        Returns:
            渲染后的图像
        """
        base_image = self.render()
        
        # 创建覆盖层
        overlay = Image.new('RGBA', base_image.size, (0, 0, 0, 0))
        draw = ImageDraw.Draw(overlay)
        
        for pos, color in overlay_data.items():
            x, y = self._tile_to_pixel(pos.col, pos.row)
            r, g, b = color
            draw.rectangle(
                [x, y, x + self.config.tile_size - 1, y + self.config.tile_size - 1],
                fill=(r, g, b, int(255 * alpha))
            )
        
        # 合并
        base_image = base_image.convert('RGBA')
        result = Image.alpha_composite(base_image, overlay)
        return result.convert('RGB')
    
    def render_route_animation(self, 
                              route_id: str,
                              num_frames: int = 30) -> List[Image.Image]:
        """
        渲染路径动画帧
        
        Args:
            route_id: 路径ID
            num_frames: 帧数
            
        Returns:
            图像帧列表
        """
        route = self.level.get_route(route_id)
        if not route or not route.points:
            return []
        
        frames = []
        base_image = self.render()
        
        for i in range(num_frames):
            frame = base_image.copy()
            draw = ImageDraw.Draw(frame)
            
            # 计算当前进度
            progress = i / (num_frames - 1)
            
            # 绘制路径进度
            self._render_route_progress(draw, route, progress)
            
            frames.append(frame)
        
        return frames
    
    def save(self, 
             output_path: str,
             mode: RenderMode = RenderMode.DETAILED,
             format: str = 'PNG') -> None:
        """
        保存渲染的地图
        
        Args:
            output_path: 输出路径
            mode: 渲染模式
            format: 图像格式
        """
        image = self.render(mode)
        image.save(output_path, format=format)
        logger.info(f"Map saved to {output_path}")
    
    def to_bytes(self, 
                 mode: RenderMode = RenderMode.DETAILED,
                 format: str = 'PNG') -> bytes:
        """
        将渲染的地图转换为字节
        
        Args:
            mode: 渲染模式
            format: 图像格式
            
        Returns:
            图像字节数据
        """
        image = self.render(mode)
        buffer = io.BytesIO()
        image.save(buffer, format=format)
        return buffer.getvalue()
    
    def _render_grid(self, draw: ImageDraw.Draw, width: int, height: int) -> None:
        """渲染网格"""
        color = self.config.color_scheme.grid_line
        
        # 垂直线
        for col in range(self.level.map_width + 1):
            x = self.config.padding + col * self.config.tile_size
            draw.line([(x, self.config.padding), 
                      (x, height - self.config.padding)],
                     fill=color, width=1)
        
        # 水平线
        for row in range(self.level.map_height + 1):
            y = self.config.padding + row * self.config.tile_size
            draw.line([(self.config.padding, y), 
                      (width - self.config.padding, y)],
                     fill=color, width=1)
    
    def _render_tiles(self, draw: ImageDraw.Draw, mode: RenderMode) -> None:
        """渲染地块"""
        for tile in self.level.tiles:
            x, y = self._tile_to_pixel(tile.position.col, tile.position.row)
            
            # 获取地块颜色
            color = self._get_tile_color(tile)
            
            # 绘制地块
            draw.rectangle(
                [x + 1, y + 1, 
                 x + self.config.tile_size - 2, 
                 y + self.config.tile_size - 2],
                fill=color
            )
            
            # 绘制部署位标记
            if self.config.show_deploy_zones:
                self._render_deploy_marker(draw, tile, x, y)
    
    def _render_deploy_marker(self, 
                              draw: ImageDraw.Draw, 
                              tile: Tile,
                              x: int, y: int) -> None:
        """渲染部署位标记"""
        if tile.can_deploy_melee() and tile.can_deploy_ranged():
            color = self.config.color_scheme.deploy_both
            marker = 'B'
        elif tile.can_deploy_melee():
            color = self.config.color_scheme.deploy_melee
            marker = 'M'
        elif tile.can_deploy_ranged():
            color = self.config.color_scheme.deploy_ranged
            marker = 'R'
        else:
            return
        
        # 绘制小圆点
        center_x = x + self.config.tile_size // 2
        center_y = y + self.config.tile_size // 2
        radius = self.config.tile_size // 6
        
        draw.ellipse(
            [center_x - radius, center_y - radius,
             center_x + radius, center_y + radius],
            fill=color
        )
    
    def _render_routes(self, draw: ImageDraw.Draw) -> None:
        """渲染路径"""
        for route in self.level.routes.values():
            if len(route.points) < 2:
                continue
            
            # 确定路径颜色
            # 检查是否是飞行路径
            is_flying = any(
                self.level.get_tile(p.position.row, p.position.col) and
                self.level.get_tile(p.position.row, p.position.col).tile_type 
                in (TileType.FLYING_START, TileType.FLYING_END)
                for p in route.points
            )
            
            color = (self.config.color_scheme.route_flying if is_flying 
                    else self.config.color_scheme.route_ground)
            
            # 绘制路径线
            points = [
                self._tile_center_to_pixel(p.position.col, p.position.row)
                for p in route.points
            ]
            
            if len(points) >= 2:
                draw.line(points, fill=color, width=3)
            
            # 绘制方向箭头
            for i in range(len(points) - 1):
                self._render_arrow(draw, points[i], points[i + 1], color)
    
    def _render_arrow(self, 
                      draw: ImageDraw.Draw,
                      start: Tuple[int, int],
                      end: Tuple[int, int],
                      color: Tuple[int, int, int]) -> None:
        """渲染方向箭头"""
        # 计算中点
        mid_x = (start[0] + end[0]) // 2
        mid_y = (start[1] + end[1]) // 2
        
        # 计算方向
        dx = end[0] - start[0]
        dy = end[1] - start[1]
        length = math.sqrt(dx * dx + dy * dy)
        
        if length < 1:
            return
        
        # 归一化
        dx /= length
        dy /= length
        
        # 箭头大小
        arrow_size = self.config.tile_size // 4
        
        # 计算箭头点
        arrow_x1 = mid_x - arrow_size * dx + arrow_size * dy * 0.5
        arrow_y1 = mid_y - arrow_size * dy - arrow_size * dx * 0.5
        arrow_x2 = mid_x - arrow_size * dx - arrow_size * dy * 0.5
        arrow_y2 = mid_y - arrow_size * dy + arrow_size * dx * 0.5
        
        draw.polygon(
            [(mid_x, mid_y), (arrow_x1, arrow_y1), (arrow_x2, arrow_y2)],
            fill=color
        )
    
    def _render_route_progress(self,
                               draw: ImageDraw.Draw,
                               route: EnemyRoute,
                               progress: float) -> None:
        """渲染路径进度"""
        if len(route.points) < 2:
            return
        
        color = self.config.color_scheme.route_ground
        points = [
            self._tile_center_to_pixel(p.position.col, p.position.row)
            for p in route.points
        ]
        
        # 计算总长度
        total_length = sum(
            math.sqrt((points[i+1][0] - points[i][0])**2 + 
                     (points[i+1][1] - points[i][1])**2)
            for i in range(len(points) - 1)
        )
        
        # 计算当前长度
        target_length = total_length * progress
        current_length = 0
        
        for i in range(len(points) - 1):
            segment_length = math.sqrt(
                (points[i+1][0] - points[i][0])**2 + 
                (points[i+1][1] - points[i][1])**2
            )
            
            if current_length + segment_length >= target_length:
                # 在此段内
                ratio = (target_length - current_length) / segment_length
                current_x = int(points[i][0] + (points[i+1][0] - points[i][0]) * ratio)
                current_y = int(points[i][1] + (points[i+1][1] - points[i][1]) * ratio)
                
                # 绘制已走过的路径
                draw.line(points[:i+1] + [(current_x, current_y)], 
                         fill=color, width=4)
                
                # 绘制当前位置标记
                radius = self.config.tile_size // 3
                draw.ellipse(
                    [current_x - radius, current_y - radius,
                     current_x + radius, current_y + radius],
                    fill=(255, 255, 255),
                    outline=color,
                    width=2
                )
                break
            
            current_length += segment_length
        else:
            # 绘制完整路径
            draw.line(points, fill=color, width=4)
    
    def _render_strategic_points(self, draw: ImageDraw.Draw) -> None:
        """渲染战略要点"""
        # 这里需要与LevelAnalyzer集成
        # 简化实现：标记路径交汇点
        from level_analyzer import PathFinder
        
        path_finder = PathFinder(self.level)
        
        # 获取所有路径
        all_paths = []
        all_paths.extend(path_finder.find_all_paths_to_end(flying=False).values())
        all_paths.extend(path_finder.find_all_paths_to_end(flying=True).values())
        
        if not all_paths:
            return
        
        # 统计交汇点
        position_count: Dict[Position, int] = {}
        for path in all_paths:
            for pos in path:
                position_count[pos] = position_count.get(pos, 0) + 1
        
        # 绘制交汇点
        for pos, count in position_count.items():
            if count > 1:
                x, y = self._tile_center_to_pixel(pos.col, pos.row)
                radius = self.config.tile_size // 3
                
                # 根据重要性调整大小
                radius = min(radius + count * 2, self.config.tile_size // 2)
                
                draw.ellipse(
                    [x - radius, y - radius, x + radius, y + radius],
                    outline=(255, 255, 0),
                    width=3
                )
    
    def _render_coordinates(self, draw: ImageDraw.Draw) -> None:
        """渲染坐标"""
        # 尝试加载字体
        try:
            font = ImageFont.truetype("arial.ttf", 10)
        except:
            font = ImageFont.load_default()
        
        text_color = self.config.color_scheme.text
        
        # 列坐标
        for col in range(self.level.map_width):
            x = self.config.padding + col * self.config.tile_size + self.config.tile_size // 2
            y = self.config.padding - 15
            draw.text((x, y), str(col), fill=text_color, font=font, anchor="mm")
        
        # 行坐标
        for row in range(self.level.map_height):
            x = self.config.padding - 15
            y = self.config.padding + row * self.config.tile_size + self.config.tile_size // 2
            draw.text((x, y), str(row), fill=text_color, font=font, anchor="mm")
    
    def _get_tile_color(self, tile: Tile) -> Tuple[int, int, int]:
        """获取地块颜色"""
        scheme = self.config.color_scheme
        
        color_map = {
            TileType.NONE: scheme.floor,
            TileType.FLOOR: scheme.floor,
            TileType.WALL: scheme.wall,
            TileType.START: scheme.start,
            TileType.END: scheme.end,
            TileType.FLYING_START: scheme.flying_start,
            TileType.FLYING_END: scheme.flying_end,
            TileType.HOLE: scheme.hole,
            TileType.HEALING: scheme.healing,
            TileType.DEFENSE: scheme.defense,
            TileType.SPEED: scheme.speed,
        }
        
        return color_map.get(tile.tile_type, scheme.floor)
    
    def _tile_to_pixel(self, col: int, row: int) -> Tuple[int, int]:
        """将网格坐标转换为像素坐标"""
        x = self.config.padding + col * self.config.tile_size
        y = self.config.padding + row * self.config.tile_size
        return (x, y)
    
    def _tile_center_to_pixel(self, col: int, row: int) -> Tuple[int, int]:
        """将网格坐标转换为像素中心坐标"""
        x = self.config.padding + col * self.config.tile_size + self.config.tile_size // 2
        y = self.config.padding + row * self.config.tile_size + self.config.tile_size // 2
        return (x, y)


class HeatmapVisualizer(MapVisualizer):
    """热力图可视化器"""
    
    def render_heatmap(self, 
                      values: Dict[Position, float],
                      colormap: str = 'viridis') -> Image.Image:
        """
        渲染热力图
        
        Args:
            values: 位置到数值的映射
            colormap: 颜色映射名称
            
        Returns:
            热力图图像
        """
        if not HAS_NUMPY:
            raise ImportError("NumPy is required for heatmap visualization")
        
        # 创建基础地图
        base_image = self.render(RenderMode.MINIMAL)
        
        # 创建热力图层
        heatmap_data = np.zeros((self.level.map_height, self.level.map_width))
        
        for pos, value in values.items():
            if 0 <= pos.row < self.level.map_height and 0 <= pos.col < self.level.map_width:
                heatmap_data[pos.row, pos.col] = value
        
        # 归一化
        if heatmap_data.max() > heatmap_data.min():
            heatmap_data = (heatmap_data - heatmap_data.min()) / (heatmap_data.max() - heatmap_data.min())
        
        # 应用颜色映射
        colored_heatmap = self._apply_colormap(heatmap_data, colormap)
        
        # 转换为图像
        heatmap_image = Image.fromarray((colored_heatmap * 255).astype(np.uint8))
        
        # 缩放到正确尺寸
        heatmap_image = heatmap_image.resize(
            (self.level.map_width * self.config.tile_size,
             self.level.map_height * self.config.tile_size),
            Image.Resampling.NEAREST
        )
        
        # 创建带透明度的覆盖层
        overlay = Image.new('RGBA', base_image.size, (0, 0, 0, 0))
        overlay.paste(heatmap_image, (self.config.padding, self.config.padding))
        
        # 调整透明度
        overlay = overlay.convert('RGBA')
        data = overlay.getdata()
        new_data = []
        for item in data:
            r, g, b, a = item
            new_data.append((r, g, b, int(a * 0.6)))  # 60% 透明度
        overlay.putdata(new_data)
        
        # 合并
        base_image = base_image.convert('RGBA')
        result = Image.alpha_composite(base_image, overlay)
        return result.convert('RGB')
    
    def _apply_colormap(self, data: np.ndarray, colormap: str) -> np.ndarray:
        """应用颜色映射"""
        # 简化的颜色映射实现
        if colormap == 'viridis':
            # Viridis 近似
            r = np.clip(0.267 + 0.105 * data + 0.63 * data**2, 0, 1)
            g = np.clip(0.004 + 0.898 * data + 0.05 * data**2, 0, 1)
            b = np.clip(0.329 + 0.644 * data - 0.667 * data**2, 0, 1)
        elif colormap == 'hot':
            # Hot
            r = np.clip(data * 3, 0, 1)
            g = np.clip((data - 0.33) * 3, 0, 1)
            b = np.clip((data - 0.66) * 3, 0, 1)
        else:
            # 灰度
            r = g = b = data
        
        return np.stack([r, g, b], axis=-1)


def visualize_level(level_data: LevelData,
                   output_path: Optional[str] = None,
                   mode: RenderMode = RenderMode.DETAILED) -> Image.Image:
    """
    便捷函数：可视化关卡
    
    Args:
        level_data: 关卡数据
        output_path: 输出路径（可选）
        mode: 渲染模式
        
    Returns:
        渲染后的图像
    """
    visualizer = MapVisualizer(level_data)
    image = visualizer.render(mode)
    
    if output_path:
        image.save(output_path)
        logger.info(f"Map visualization saved to {output_path}")
    
    return image


if __name__ == '__main__':
    # 示例用法
    logging.basicConfig(level=logging.INFO)
    
    from level_analyzer import LevelDataLoader
    
    # 创建示例关卡
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
        'waves': [],
    }
    
    # 加载关卡
    level = LevelDataLoader.load_from_dict(example_level)
    
    # 可视化
    visualizer = MapVisualizer(level)
    image = visualizer.render(RenderMode.DETAILED)
    
    # 显示或保存
    image.show()
    # image.save('map_visualization.png')
