import sys, os
from PIL import Image, ImageDraw
d = sys.argv[1]; os.makedirs(d, exist_ok=True)
im = Image.new("RGB", (200, 120)); dr = ImageDraw.Draw(im)
dr.rectangle([0, 0, 99, 59], fill=(255, 0, 0)); dr.rectangle([100, 0, 199, 59], fill=(0, 255, 0))
dr.rectangle([0, 60, 99, 119], fill=(0, 0, 255)); dr.rectangle([100, 60, 199, 119], fill=(255, 255, 255))
im.save(d + "/a.png"); im.save(d + "/a.jpg", quality=90); im.save(d + "/a.gif")
im.convert("P", palette=Image.ADAPTIVE, colors=16).save(d + "/p.png"); im.convert("L").save(d + "/l.png")
im.convert("RGBA").save(d + "/rgba.png"); im.save(d + "/prog.jpg", quality=85, progressive=True)
im.save(d + "/sub.jpg", quality=85, subsampling=2)
big = Image.effect_noise((900, 700), 80).convert("RGB"); big.save(d + "/big.png"); big.save(d + "/big.jpg")
