class MouseManager:
    def __init__(self):
        self.x = 0
        self.y = 0
        self.buttons = {'LEFT': False, 'RIGHT': False, 'MIDDLE': False}
    
    def move(self, x, y):
        self.x = x
        self.y = y
        
    def get_position(self):
        return (self.x, self.y)
