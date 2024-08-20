#!/usr/bin/env python
# -*- coding:utf-8 -*-
# @Date   :2021/12/30
# @Author :p_pyjli
# @File   :logger.py
# @Desc   :


import os
import logging

logDir = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'log', 'monitor.log')


class Logger(object):
    def __init__(self, path):
        self.path = path

        self.logger = logging.getLogger(__file__)
        self.logger.setLevel(logging.DEBUG)

        # 建立一个filehandler来把日志记录在文件里，级别为debug以上
        fh = logging.FileHandler(self.path)
        fh.setLevel(logging.DEBUG)

        # 建立一个streamhandler来把日志打在CMD窗口上，级别为debug以上
        ch = logging.StreamHandler()
        ch.setLevel(logging.DEBUG)

        # 设置日志格式
        # formatter = logging.Formatter('[%(asctime)s] - {"date": "%(asctime)s", "name": "%(name)s", "level": "%(levelname)s", "line": %(lineno)s, "message": "%(message)s"}', datefmt="%Y-%m-%d %H:%M:%S")
        formatter = logging.Formatter(
            '[%(asctime)s] - {"date": "%(asctime)s", "funcname": "%(funcName)s", "level": "%(levelname)s", "line": %(lineno)s, "message": "%(message)s"}',
            datefmt="%Y-%m-%d %H:%M:%S")
        ch.setFormatter(formatter)
        fh.setFormatter(formatter)

        # 将相应的handler添加在logger对象中
        self.logger.addHandler(ch)
        self.logger.addHandler(fh)

    # 回掉logger
    def log(self):
        return self.logger


# 外部使用直接调用引入log，使用为：log.info("message")
log = Logger(logDir).log()
