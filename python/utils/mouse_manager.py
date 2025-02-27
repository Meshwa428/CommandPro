class MouseManager:
    def __init__(self):
        self.x = 0
        self.y = 0
        self.current_position = None
        self.buttons = {'LEFT': False, 'RIGHT': False, 'MIDDLE': False}
    
    def move(self, x, y):
        self.x = x
        self.y = y
        self.current_position = (x, y)

    def get_position(self):
        return (self.x, self.y)
