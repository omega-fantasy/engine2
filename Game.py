from Engine import *

class Map:
    class Config:
        class Biome:
            def __init__(self, name, max_temp, name_wall, max_height, wall_height, blocking):
                self._vegetation = []
                self._name = name
                self._max_temp = max_temp
                self._name_wall = name_wall
                self._max_height = max_height
                self._wall_height = wall_height
                self._blocking = blocking
            def add_vegegation(self, name, quantity):
                self._vegetation.append((name, quantity))
        class Elevation:
            def __init__(self, q):
                self._biomes = []
                self._quantity = q
            def add_biome(self, name:str, max_temp=100, name_wall="", max_height=0, wall_height=0, blocking=False):
                b = Map.Config.Biome(name, max_temp, name_wall, max_height, wall_height, blocking)
                self._biomes.append(b)
                return b
        def __init__(self, map_size:int, tile_size:int):
            self._map_size = map_size
            self._tile_size = tile_size
            self._sample_factor = 3
            self._sample_distance = 6
            self._num_cells = 16
            self._elevations = []
        def add_elevation_level(self, quantity):
            new_elevation = Map.Config.Elevation(quantity)
            self._elevations.append(new_elevation)
            return new_elevation
        def set_parameters(self, num_cells, sample_factor, sample_distance):
            self._num_cells = num_cells
            self._sample_factor = sample_factor
            self._sample_distance = sample_distance

    def create(mapconfig:Config, width, height, parent, xpos, ypos):
        global _map
        _map = TilemapWidget(width, height, mapconfig._map_size, mapconfig._map_size, 
                            mapconfig._tile_size, mapconfig._tile_size, parent, xpos, ypos)
        _map._cfg_set_parameters(mapconfig._num_cells, mapconfig._sample_factor, mapconfig._sample_distance)
        elev_idx = 0
        for elevation in mapconfig._elevations:
            _map._cfg_add_elevation(elevation._quantity)
            for biome in elevation._biomes:
                _map._cfg_add_biome(elev_idx, biome._name, biome._max_temp, biome._name_wall,
                                    biome._max_height, biome._wall_height, biome._blocking)
                for vegetation in biome._vegetation:
                    _map._cfg_add_vegetation(elev_idx, biome._name, vegetation[0], vegetation[1])
            elev_idx += 1

        _map._randomize_map()
        accel = 10
        Engine.bind_key("Up", lambda: _map.move_camera(0, -accel))
        Engine.bind_key("Down", lambda: _map.move_camera(0, accel))
        Engine.bind_key("Left", lambda: _map.move_camera(-accel, 0))
        Engine.bind_key("Right", lambda: _map.move_camera(accel, 0))
        Engine.bind_key("WheelDown", lambda: _map.zoomout())
        Engine.bind_key("WheelUp", lambda: _map.zoomin())

class Hud:
    class Menu:
        def __init__(self):
            pass

    def create(width, height, parent, xpos, ypos):
        global _hud_widget
        global _hud_buttons
        _hud_widget = GameWidget(width, height, parent, xpos, ypos)
        _hud_widget._set_texture(TextureGenerator.box(int(width), int(height)))
        _hud_buttons = []
    def add_menu(menu):
        pass
    def add_widget():
        pass

def create_map_config(map_size, tile_size):
    cfg = Map.Config(map_size, tile_size)
    cfg.set_parameters(num_cells=16, sample_factor=3, sample_distance=6)
    cfg.add_elevation_level(quantity=0.57).add_biome("water", blocking=True)
    cfg.add_elevation_level(quantity=0.0175).add_biome("beach")
    ground_level = cfg.add_elevation_level(quantity=0.11)
    ground_level.add_biome("snow", max_temp=30)
    ground_level.add_biome("grass", max_temp=70)
    ground_level.add_biome("sand", max_temp=100)
    cfg.add_elevation_level(quantity=0.01).add_biome("earth")
    cfg.add_elevation_level(quantity=0.2925).add_biome("earth", name_wall="mountain_wall", max_height=32, wall_height=2)
    return cfg

def init_ingame():
    map_size = 1024
    tile_size = 16
    hud_width = 0.3
    config = create_map_config(map_size, tile_size)
    Map.create(config, (1-hud_width)*Engine.screen_width(), Engine.screen_height(), None, hud_width*Engine.screen_width(), 0) 
    Hud.create(hud_width*Engine.screen_width(), Engine.screen_height(), None, 0, 0) 
