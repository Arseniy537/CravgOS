#!/usr/bin/env python3
"""Конвертирует снимки экрана тестов (/tmp/shots/*.ppm) в PNG для просмотра."""
import glob, os
from PIL import Image
for p in sorted(glob.glob('/tmp/shots/*.ppm')):
    Image.open(p).save(p[:-4] + '.png'); print(p[:-4] + '.png')
