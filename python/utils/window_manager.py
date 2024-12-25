class Window:
    def __init__(self, name, x=0, y=0, width=0, height=0):
        self.name = name
        self.x = x
        self.y = y
        self.width = width
        self.height = height
        self.focused = False

class WindowManager:
    def __init__(self):
        self.windows = {}
        
    def create_window(self, name, x=0, y=0, width=0, height=0):
        self.windows[name] = Window(name, x, y, width, height)
        
    def exists(self, name):
        return name in self.windows
        
    def focus(self, name):
        if not self.exists(name):
            return False
        # Unfocus all windows
        for window in self.windows.values():
            window.focused = False
        # Focus requested window
        self.windows[name].focused = True
        return True
        
    def move(self, name, x, y):
        if not self.exists(name):
            return False
        self.windows[name].x = x
        self.windows[name].y = y
        return True
