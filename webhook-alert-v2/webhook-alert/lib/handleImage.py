#!/usr/bin/env python
# -*- coding:utf-8 -*-
# @Date   :2022/1/5
# @Author :p_pyjli
# @File   :handleImage.py
# @Desc   : 用于处理图片，根据图片的路径或者url获取图片的md5，以及数据


import os
import base64
import hashlib
from lib.logger import log
from urllib.request import urlretrieve


def get_url_image_info(image_url):
    """
    根据图片url获取图片的base64数据，和图片的md5数据值
    :param image_url: 图片网络地址
    :return: 返回图片的base64数据，md5值
    """
    try:
        log.info(f"Start download image. URL：{image_url}")
        urlretrieve(image_url, './img.png')
        try:
            log.info("Start get image base64 data. ")
            with open('./img.png', 'rb') as file:
                data = file.read()
                encodestr = base64.b64encode(data)
                image_data = str(encodestr, 'utf-8')

            log.info("Start get image md5. ")
            with open('./img.png', 'rb') as file:
                md = hashlib.md5()
                md.update(file.read())
                image_md5 = md.hexdigest()

            os.remove('./img.png')
            log.info(f"Get image base64 and md5 success... ")
            return {'image_data': image_data, 'image_md5': image_md5}
        except:
            log.error(f"Get image base64 and md5 failed ! ")
            exit(0)
    except:
        log.error("Image download failed ! ")
        exit(0)


def get_local_image_info(image_path):
    """
    @param image_path:
    @return:
    """
    if os.path.exists(image_path):
        try:
            log.info('Get image base64 data. ')
            with open(image_path, 'rb') as file:
                data = file.read()
                encodestr = base64.b64encode(data)
                image_data = str(encodestr, 'utf-8')

            log.info('Get image md5. ')
            with open(image_path, 'rb') as file:
                md = hashlib.md5()
                md.update(file.read())
                image_md5 = md.hexdigest()
            log.info(f'Get image {image_path} base64 and md5 success... ')
            return {'image_data': image_data, 'image_md5': image_md5}
        except:
            log.error(f'Get image {image_path} base64 and md5 failed ! ')
            exit(0)
    else:
        log.error(f'Image path {image_path} does not exist ! ')
        exit(0)

