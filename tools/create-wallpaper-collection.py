#!/usr/bin/env python3
"""Render original, palette-matched geometric wallpapers; never modify upstream art."""
# SPDX-License-Identifier: MIT
import json, math, re
from pathlib import Path
from PIL import Image, ImageDraw, ImageColor
root=Path(__file__).resolve().parents[1]
raw=(root/'src/palettes.cpp').read_text().split('R"PALETTES(')[1].split(')PALETTES"')[0]
palettes={p['id']:p for p in json.loads(raw)}
for key,bg,accent in [('caelestia','#181622','#c4a7ff'),('ryoku','#151515','#e6a0b2'),('windows','#142335','#0078d4'),('breeze','#eff0f1','#3daee9')]:
 palettes[key]={'background':bg,'accent':accent,'green':accent,'magenta':accent,'lighter_background':bg}
folder=root/'assets/wallpapers';catalog=json.loads((folder/'catalog.json').read_text())
for key,name in [('omarchy-rose-pine','rose-pine__3-omarchy-plants.webp'),('omarchy-lumon','lumon__02-opinions-equally.webp')]:
 if name in catalog[key]:catalog[key].remove(name)
# Keep the first five existing works except Retro and Tokyo Night, which keep their extras.
for key,palette in palettes.items():
 names=catalog.setdefault(key,[])
 if key not in ('omarchy-retro-82','omarchy-tokyo-night'):names[:]=names[:5]
 for n in range(len(names),5):
  design=n%5;name=f'{key}__studio-{design+1}.png'
  bg=ImageColor.getrgb(palette['background']);ac=ImageColor.getrgb(palette['accent']);alt=ImageColor.getrgb(palette.get('magenta',palette['accent']))
  def mix(c,t):return tuple(round(bg[i]*(1-t)+c[i]*t) for i in range(3))
  im=Image.new('RGB',(2560,1440),bg);d=ImageDraw.Draw(im)
  # Distinct compositions: orbit arcs, mountain layers, ribbons, tiles and dusk.
  if design==0:
   for r in range(1600,100,-65):d.ellipse((1700-r,540-r,1700+r,540+r),outline=mix(ac,.2+.65*(1-r/1600)),width=18)
  elif design==1:
   for layer in range(6):
    pts=[(0,1440)]+[(x,int(530+layer*140+100*math.sin(x/350+layer)+70*math.cos(x/170+layer))) for x in range(0,2561,16)]+[(2560,1440)]
    d.polygon(pts,fill=mix(ac,.14+layer*.12))
  elif design==2:
   for layer in range(9):
    pts=[(x,int(300+layer*120+200*math.sin(x/600+layer*.35))) for x in range(-50,2610,10)]
    d.line(pts,fill=mix(alt,.2+layer*.07),width=55)
  elif design==3:
   for y in range(-100,1600,220):
    for x in range(-100,2700,220):
     shift=110 if (y//220)%2 else 0
     d.rounded_rectangle((x+shift,y,x+shift+180,y+180),radius=45,fill=mix(ac,.12+.5*((x+y)%900)/900))
  else:
   d.ellipse((1490,220,2090,820),fill=mix(alt,.75))
   for layer in range(5):
    pts=[(0,1440)]+[(x,int(830+layer*125+120*math.sin(x/700+layer))) for x in range(0,2561,16)]+[(2560,1440)]
    d.polygon(pts,fill=mix(ac,.15+layer*.1))
  im.save(folder/name,optimize=True);names.append(name)
(folder/'catalog.json').write_text(json.dumps(catalog,indent=2)+'\n')
print('Themes:',len(catalog),'Selectable wallpapers:',sum(map(len,catalog.values())))
