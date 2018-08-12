from tkinter import *
from PIL import ImageTk, Image
import os
import requests
from io import BytesIO

# Config
width = 1440
height = 900
ratio = min(width / 550, height / 512)
url = 'https://api.buienradar.nl/image/1.0/radarmapbe?width=550'
update_interval = 60 * 1000

def update_map():
    try:
        response = requests.get(url)
        try:
            im = Image.open(BytesIO(response.content))
        except OSError:
            im = Image.open('error.png')
    except:
        im = Image.open('error.png')

    try:
        i = 0
        while 1:
            im.seek(i)
            imframe = im.copy()
            i += 1
    except EOFError:
        imframe = imframe.resize((int(550 * ratio), int(512 * ratio)))
        return ImageTk.PhotoImage(imframe)

def close(event):
    root.withdraw()
    sys.exit()

def refresh_map():
    map = update_map()
    panel.configure(image = map)
    panel.image = map
    root.after(update_interval, refresh_map)

# Go fullscreen and hide mouse cursor
root = Tk()
root.attributes("-fullscreen", True)
root.config(cursor="none")

# Bind any mouse or keyboard button press to close program
root.bind('<ButtonPress>', close)
root.bind('<KeyPress>', close)

# Schedule map update task
root.after(update_interval, refresh_map)

# Load map for the first time
map = update_map()

# Display map
panel = Label(root, image = map)
panel.configure(background='black')
panel.pack(side = "bottom", fill = "both", expand = "yes")

root.mainloop()
