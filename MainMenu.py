from Engine import *
from Game import init_ingame

def init_assets():
    # Textures
    dim = 16
    TextureGenerator.noise("water", dim, dim, Color(0, 72, 200), 0.04)
    TextureGenerator.noise("grass", dim, dim, Color(67, 141, 13), 0.03)
    TextureGenerator.noise("sand", dim, dim, Color(220, 196, 126), 0.02)
    TextureGenerator.noise("beach", dim, dim, Color(194, 152, 88), 0.02)
    TextureGenerator.noise("earth", dim, dim, Color(155, 112, 57), 0.03)
    TextureGenerator.noise("snow", dim, dim, Color(180, 180, 220), 0.01)
    TextureGenerator.noise("mountain", dim, dim, Color(175, 127, 64), 0.03)
    TextureGenerator.noise("mountain_wall", dim, dim, Color(104, 64, 32), 0.1)

    # Audio
    Engine.load_sound("./click.wav", "click")



class MainMenu(GameWidget):
    class NewButton(GameButton):
        def __init__(self, width, height, parent, off_x, off_y):
            super().__init__(width, height, "New Game", parent, off_x, off_y, self.on_click)
            self._parent = parent
        def on_click(self):
            super().on_click()
            print("new game!")
            self._parent.remove()
            init_ingame()

    class LoadButton(GameButton):
        def __init__(self, width, height, parent, off_x, off_y):
            super().__init__(width, height, "Load Game", parent, off_x, off_y, self.on_click)
        def on_click(self):
            super().on_click()
            print("load game!")
    
    class OptionsButton(GameButton):
        def __init__(self, width, height, parent, off_x, off_y):
            super().__init__(width, height, "Options", parent, off_x, off_y, self.on_click)
        def on_click(self):
            super().on_click()
            print("options!")
    
    class QuitButton(GameButton):
        def __init__(self, width, height, parent, off_x, off_y):
            super().__init__(width, height, "Quit", parent, off_x, off_y, self.on_click)
        def on_click(self):
            super().on_click()
            print("quit!")
            #g_locals = locals()
            #code.interact(local=g_locals)

    def __init__(self):
        width = 0.5 * Engine.screen_width()
        height = 0.5 * Engine.screen_height()
        super().__init__(width, height, parent=None, offset_x=0.25*Engine.screen_width(), offset_y=0.25*Engine.screen_height())
        self._buttons = [
            MainMenu.NewButton(width, 0.22 * height, self, 0, 0),  
            MainMenu.LoadButton(width, 0.22 * height, self, 0, 0.25 * height),  
            MainMenu.OptionsButton(width, 0.22 * height, self, 0, 0.5 * height),  
            MainMenu.QuitButton(width, 0.22 * height, self, 0, 0.75 * height)
        ]

Engine.init(1920, 1080)
init_assets()
m = MainMenu()
Engine.run()
