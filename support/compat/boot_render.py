#!/usr/bin/env python3
"""Render the boot layout and selection assets from shared design tokens."""
from pathlib import Path
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
TOKENS = json.loads((ROOT / 'design/tokens.json').read_text())

def rgba(hex_colour, opacity):
    rgb = [int(hex_colour[i:i+2], 16) for i in (1, 3, 5)]
    return 'rgba(%d,%d,%d,%s)' % (*rgb, opacity)

def render(destination, source, size, background, foreground, accent):
    folder = Path(destination)
    width, height = map(int, size.split('x'))
    spacing, radius = TOKENS['spacing'], TOKENS['radius']
    # Geometry is shared by the baked glass panel and the GRUB text/menu.
    x, y, right, bottom = width * .18, height * .12, width * .82, height * .88
    panel_radius = round(radius['panel'] * height / 800)
    subprocess.run(['magick', source, '-auto-orient', '-resize', size+'^', '-gravity', 'center',
        '-extent', size, '-colorspace', 'sRGB', '-blur', '0x8', '-fill', rgba(background, .48),
        '-draw', f'rectangle 0,0 {width},{height}', '-fill', rgba(background, TOKENS['material']['opacity']),
        '-stroke', rgba(foreground, .21), '-strokewidth', '1',
        '-draw', f'roundrectangle {x},{y} {right},{bottom} {panel_radius},{panel_radius}',
        '-strip', '-quality', '92', str(folder / 'grub-background.jpg')], check=True)
    font = 32 if height >= 1200 else 24 if height >= 800 else 16
    caption = 24 if height >= 1200 else 16
    row = font + spacing['lg'] * 2
    theme = f'''# Generated from the Anto Desktop shared design tokens.
title-text: ""
desktop-image: "background.jpg"
desktop-color: "{background}"
terminal-font: "Terminus Regular 18"
+ label {{
  left = 22%
  top = 17%
  width = 56%
  align = "center"
  text = "Il tuo desktop"
  font = "Unifont Regular {font}"
  color = "{foreground}"
}}
+ label {{
  left = 22%
  top = 23%
  width = 56%
  align = "center"
  text = "Scegli il sistema da avviare"
  font = "Unifont Regular {caption}"
  color = "{foreground}"
}}
+ boot_menu {{
  left = 22%
  top = 33%
  width = 56%
  height = 36%
  item_font = "Unifont Regular {font}"
  item_color = "{foreground}"
  selected_item_color = "{foreground}"
  icon_width = {font + spacing['lg']}
  icon_height = {font + spacing['lg']}
  item_icon_space = {spacing['lg']}
  item_height = {row}
  item_padding = {spacing['lg']}
  item_spacing = {spacing['sm']}
  selected_item_pixmap_style = "select_*.png"
}}
+ label {{
  left = 22%
  top = 75%
  width = 56%
  align = "center"
  id = "__timeout__"
  text = "Avvio automatico tra %d secondi"
  font = "Unifont Regular {caption}"
  color = "{accent}"
}}
+ label {{
  left = 22%
  top = 81%
  width = 56%
  align = "center"
  text = "Frecce: scegli   Invio: avvia   E: modifica   C: console"
  font = "Unifont Regular 16"
  color = "{foreground}"
}}
'''
    (folder / 'grub-theme.txt').write_text(theme)
    # A real nine-slice box: corners are fixed, edges/centre stretch.
    corner = radius['control']
    extent = corner * 2 + 1
    whole = folder / 'selection.png'
    subprocess.run(['magick', '-size', f'{extent}x{extent}', 'xc:none', '-fill', rgba(accent, .28),
        '-stroke', rgba(foreground, .21), '-strokewidth', '1',
        '-draw', f'roundrectangle 0,0 {extent-1},{extent-1} {corner},{corner}', 'PNG32:'+str(whole)], check=True)
    cuts = {'nw':(0,0,corner,corner), 'n':(corner,0,1,corner), 'ne':(corner+1,0,corner,corner),
            'w':(0,corner,corner,1), 'c':(corner,corner,1,1), 'e':(corner+1,corner,corner,1),
            'sw':(0,corner+1,corner,corner), 's':(corner,corner+1,1,corner), 'se':(corner+1,corner+1,corner,corner)}
    for name, (cx,cy,cw,ch) in cuts.items():
        subprocess.run(['magick', str(whole), '-crop', f'{cw}x{ch}+{cx}+{cy}', '+repage',
                        'PNG32:'+str(folder / f'select_{name}.png')], check=True)
    whole.unlink()

if __name__ == '__main__':
    render(*sys.argv[1:])
