#!/usr/bin/env python
# -*- coding:utf-8 -*-
# @Date   :2021/12/30
# @Author :p_pyjli
# @File   :run.py
# @Desc   :


import os
import sys

BASE_PATH = os.path.dirname(os.path.dirname(__file__))
sys.path.append(BASE_PATH)

from core import main

if __name__ == '__main__':
    main.start()
