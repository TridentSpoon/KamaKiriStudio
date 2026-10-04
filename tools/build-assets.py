#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate original Plasma SVG frames. KDE's installed Breeze supplies other elements."""
from pathlib import Path
import json
root=Path(__file__).resolve().parents[1]/'assets/desktoptheme'
for look,radius in [('caelestia',16),('fluent11',8),('fluent10',0)]:
 folder=root/('kamakiri-'+look);folder.mkdir(parents=True,exist_ok=True)
 (folder/'metadata.json').write_text(json.dumps({'KPlugin':{'Id':'kamakiri-'+look,'Name':'Kamakiri style' if look=='caelestia' else 'KamaKiri '+look,'Version':'0.5.0','License':'MIT','Authors':[{'Name':'KamaKiriStudio contributors'}]},'X-Plasma-API':'5.0'},indent=2))
 (folder/'plasmarc').write_text('[Settings]\nFallbackTheme=default\n\n[ContrastEffect]\nenabled=true\ncontrast=1.0\nintensity=1.0\nsaturation=1.0\n\n[BlurBehindEffect]\nenabled=true\n\n[AdaptiveTransparency]\nenabled=false\n')
 def svg(prefix,r):
  t=max(1,r);parts=['<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128"><defs><style type="text/css">.ColorScheme-Background {color:#202020;} .ColorScheme-Text {color:#f4f4f4;}</style></defs>']
  for pre in [prefix, 'mask-'+prefix]:
   fill='white' if pre.startswith('mask') else 'currentColor';cls='' if pre.startswith('mask') else ' class="ColorScheme-Background"'
   for side,x,y,w,h in [('center',t,t,32,32),('top',t,0,32,t),('bottom',t,t+32,32,t),('left',0,t,t,32),('right',t+32,t,t,32)]:
    parts.append(f'<rect id="{pre}{side}"{cls} x="{x}" y="{y}" width="{w}" height="{h}" fill="{fill}"/>')
   for name,x,y,rot in [('topleft',0,0,0),('topright',t+32,0,90),('bottomright',t+32,t+32,180),('bottomleft',0,t+32,270)]:
    path=f'M {t} 0 Q 0 0 0 {t} L {t} {t} Z' if r else f'M 0 0 H {t} V {t} H 0 Z'
    parts.append(f'<path id="{pre}{name}"{cls} transform="translate({x},{y}) rotate({rot},{t/2},{t/2})" d="{path}" fill="{fill}"/>')
  for side in ['top','bottom','left','right']:parts.append(f'<rect id="{prefix}hint-{side}-margin" x="90" y="90" width="{max(4,t)}" height="{max(4,t)}" fill="black"/>')
  parts.append(f'<rect id="{prefix}hint-stretch-borders" x="100" y="100" width="1" height="1"/>')
  return ''.join(parts)
 for path,rad in [('widgets/panel-background.svg',radius if look=='caelestia' else 0),('dialogs/background.svg',radius)]:
  target=folder/path;target.parent.mkdir(exist_ok=True);target.write_text(svg('',rad)+ '</svg>')
