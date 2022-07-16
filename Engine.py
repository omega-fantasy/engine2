from ctypes import *
import code
import struct
import random

class TextureGenerator:
    def _reg_bitmap(name, bitmap, width, height):
        arr_c = (c_int * len(bitmap))(*bitmap)
        ENG.texture_from_bitmap(name.encode('utf-8'), arr_c, width, height)

    def box(width, height):
        t_name = "box_"+str(width)+"_"+str(height)
        if not ENG.texture_registered(t_name):
            bitmap = []
            color_topleft = Color(0, 0, 150)
            color_botright = Color(0, 0, 32)
            color_border = Color(180, 180, 180)
            color_outborder = color_border.copy()
            color_outborder.sub(80)
            border_width = 6
            bw = border_width
            d1 = 0.25 * border_width;
            d2 = 0.75 * border_width;
            d3 = border_width;
            max_dist = width + height - 2
            for y in range(height):
                for x in range(width):
                    if x < d1 or x > width - d1 - 1 or y < d1 or y > height - d1 - 1:
                        bitmap.append(color_outborder.pack()) 
                    elif (x >= d1 and x <= d2) or (x >= width - d2) or (y >= d1 and y <= d2) or (y >= height - d2): 
                        bitmap.append(color_border.pack()) 
                    elif (x > d2 and x < bw) or (x < width - d2 and x > width - bw - 1) or (y > d2 and y < bw) or (y < height - d2 and y > height - bw - 1):
                        bitmap.append(color_outborder.pack()) 
                    else:
                        p_topleft = 1 - (x + y) / max_dist 
                        p_botright = 1 - (width - x + height + y) / max_dist 
                        c = Color(0, 0, 0, 255)
                        c.red = p_topleft * color_topleft.red + p_botright * color_botright.red
                        c.green = p_topleft * color_topleft.green + p_botright * color_botright.green
                        c.blue = p_topleft * color_topleft.blue + p_botright * color_botright.blue
                        c._correct()
                        bitmap.append(c.pack())
            TextureGenerator._reg_bitmap(t_name, bitmap, width, height)
        return t_name

    def noise(name, width, height, color, variance):
        if not ENG.texture_registered(name):
            bitmap = []
            for i in range(height * width):
                c = color.copy()
                g = random.gauss(0, variance)
                if g < 0:
                    c.sub(255 * -g)
                else:
                    c.add(255 * g)
                bitmap.append(c.pack())
            TextureGenerator._reg_bitmap(name, bitmap, width, height)
        return name

class Color:
    def __init__(self, red, green, blue, alpha=255):
        self.red = red
        self.green = green
        self.blue = blue
        self.alpha = alpha
        self._correct()
    def pack(self):
        return struct.unpack("i", struct.pack("BBBB", int(self.blue), int(self.green), int(self.red), int(self.alpha)))[0]
    def copy(self):
        return Color(self.red, self.green, self.blue, self.alpha)
    def add(self, amount):
        self.red += amount 
        self.blue += amount 
        self.green += amount 
        self._correct()
    def sub(self, amount):
        self.red -= amount 
        self.blue -= amount 
        self.green -= amount 
        self._correct()
    def _correct(self):
        self.red = min(255, max(0, int(self.red)))
        self.green = min(255, max(0, int(self.green)))
        self.blue = min(255, max(0, int(self.blue)))
        self.alpha = min(255, max(0, int(self.alpha)))

class Engine:
    def init(width, height):
        global ENG 
        ENG = cdll.LoadLibrary("./libEngine.so")
        ENG.texture_from_bitmap.argtypes = [c_char_p, POINTER(c_int), c_int, c_int]
        ENG.texture_registered.restype = c_bool
        ENG.init(width, height)
        global _screen_width
        global _screen_height
        _screen_width = width
        _screen_height = height
        global _key_actions
        _key_actions = []

    def screen_width():
        return _screen_width
    def screen_height():
        return _screen_height

    def bind_key(key, action):
        CBFUNC = CFUNCTYPE(c_void_p)
        _key_actions.append(CBFUNC(action))
        ENG.bind_key(key.encode('utf-8'), _key_actions[-1])

    def run():
        ENG.run()

    def load_sound(path, name):
        ENG.load_sound(path.encode('utf-8'), name.encode('utf-8'))
    
    def play_sound(name):
        ENG.play_sound(name.encode('utf-8'))

    class UIElement:
        def __init__(self, width, height):
            self._ptr = ENG.create_widget(int(width), int(height))
            self._width = width
            self._height = height
        def _set_text(self, txt, txt_height, off_x, off_y):
            ENG.set_widget_text(self._ptr, txt.encode('utf-8'), int(txt_height), int(off_x), int(off_y))
        def _set_parent(self, parent, off_x, off_y):
            if parent is None:
                parent = 0
            else:
                parent = parent._ptr
            ENG.add_widget(parent, self._ptr, int(off_x), int(off_y))
        def _set_texture(self, texture_name):
            ENG.set_widget_texture(self._ptr, texture_name.encode('utf-8'))
        def _set_callback(self, cb):
            CBFUNC = CFUNCTYPE(c_void_p)
            self._cb = CBFUNC(cb) 
            ENG.set_widget_callback(self._ptr, self._cb)
        def width(self):
            return self._width
        def height(self):
            return self._height
        def remove(self):
            ENG.remove_widget(self._ptr)
            self._ptr = 0

class GameWidget(Engine.UIElement):
    def __init__(self, width, height, parent=None, offset_x=0, offset_y=0):
        super().__init__(width, height)
        self._set_parent(parent, offset_x, offset_y)

class GameButton(GameWidget):
    def __init__(self, width, height, text=None, parent=None, offset_x=0, offset_y=0, callback=None):
        super().__init__(width, height, parent, offset_x, offset_y)
        self._create_texture(width, height)
        if text is not None:
            self._set_text(text, 0.4 * height, 0.1 * width, 0.25 * height)
        if callback is not None:
            self._set_callback(callback)
    def on_click(self):
        Engine.play_sound("click")
    def _create_texture(self, width, height):
        t_name = TextureGenerator.box(int(width), int(height))
        self._set_texture(t_name)

class TilemapWidget(GameWidget):
    def __init__(self, widget_width, widget_height, map_width, map_height, tile_width, tile_height, parent=None, offset_x=0, offset_y=0):
        self._ptr = ENG.create_tilemap_widget(int(widget_width), int(widget_height), int(map_width), int(map_height), int(tile_width), int(tile_height))
        self._width = widget_width
        self._height = widget_height
        self._set_parent(parent, offset_x, offset_y)
    def set_tile(self, x, y, texture_name, ground=False):
        ENG.set_tile(x, y, texture_name.encode('utf-8'), ground)
    def move_camera(self, x_amount, y_amount):
        ENG.tilemap_move(x_amount, y_amount)
    def zoomin(self):
        ENG.tilemap_zoomin()
    def zoomout(self):
        ENG.tilemap_zoomout()
    def _cfg_add_elevation(self, q):
        ENG.mapconfig_add_elevation.argtypes = [c_float]
        ENG.mapconfig_add_elevation(q)
    def _cfg_add_biome(self, elevation, name, max_temp, name_wall, max_height, wall_height, blocking):
        #ENG.mapconfig_add_biome.argtypes = [c_short, c_char_p, c_char_p, c_float]
        ENG.mapconfig_add_biome(elevation, name.encode('utf-8'), max_temp, name_wall.encode('utf-8'), max_height, wall_height, blocking)
    def _cfg_add_vegetation(self, elevation, biome, vegetation, quantity):
        ENG.mapconfig_add_elevation.argtypes = [c_short, c_char_p, c_char_p, c_float]
        ENG.mapconfig_add_vegetation(elevation, biome.encode('utf-8'), vegetation.encode('utf-8'), quantity)
    def _cfg_set_parameters(self, num_cells, sample_distance, sample_factor):
        ENG.mapconfig_set_parameters(num_cells, sample_distance, sample_factor)
    def _randomize_map(self):
        ENG.tilemap_randomize()