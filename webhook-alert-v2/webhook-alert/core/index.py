#!/usr/bin/env python
# -*- coding:utf-8 -*-
# @Date   :2022/1/10
# @Author :p_pyjli
# @File   :index.py
# @Desc   :首页


from flask import Blueprint

index_opt = Blueprint('index_opt', __name__)


@index_opt.route('/', methods=['get'])
def index():
    return '<h1 align="center">Hello World</h1>'





