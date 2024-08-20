#!/usr/bin/env python
# -*- coding:utf-8 -*-
# @Date   :2021/12/30
# @Author :p_pyjli
# @File   :wechat.py
# @Desc   :通过企业微信群机器人，进行告警消息发送


import requests
import json
from lib.logger import log


class Wechat(object):
    """
    发送信息到企业微信的类
    """
    headers = {'Content-Type': 'application/json;charset=utf-8'}

    def __init__(self, webhook):
        self.webhook = webhook

    def __log(self, ret, msg):
        if ret.get('errcode') == 0:
            log.info(f'{msg}  Send To Wechat Success...')
        else:
            log.error(f'{msg}  Send To Wechat Failed... Error Message: {ret["errmsg"]}')

    def text(self, msg):
        """
        发送文本消息信息
        :param msg: 消息文本内容
        :return:
        """
        data = {
            "msgtype": "text",
            "text": {
                "content": msg
            }
        }
        ret = requests.post(url=self.webhook, data=json.dumps(data), headers=self.headers).json()
        self.__log(ret, msg)

    def text_user(self, msg, users):
        """
        发送文本消息信息
        :param msg: 消息文本内容
        :return:
        """
        data = {
            "msgtype": "text",
            "text": {
                "content": msg,
                "mentioned_list": users
            }
        }
        ret = requests.post(url=self.webhook, data=json.dumps(data), headers=self.headers).json()
        self.__log(ret, msg)

    def markdown(self, msg):
        """
        发送markdown类型的消息
        :param msg: markdown格式的文本
        :return:
        """
        data = {
            "msgtype": "markdown",
            "markdown": {
                "content": msg
            }
        }
        ret = requests.post(url=self.webhook, data=json.dumps(data), headers=self.headers).json()
        self.__log(ret, msg)

    def markdown_user(self, msg, users):
        """
        发送markdown类型的消息，并@用户
        :param msg: markdown格式的文本
        :return:
        """
        content = msg + "\n"
        for u in users:
            content += f"<@{u}>"

        data = {
            "msgtype": "markdown",
            "markdown": {
                "content": content
            }
        }
        ret = requests.post(url=self.webhook, data=json.dumps(data), headers=self.headers).json()
        self.__log(ret, msg)

    def image(self, msg):
        """
        发送图片消息
        :param msg: 图片消息字典，需包含image_data、image_md5两个字段。格式为：{'image_data': '图片文件的base64数据', 'image_md5': '图片文件的md5值'}
        :return:
        """
        data = {
            "msgtype": "image",
            "image": {
                "base64": msg['image_data'],
                "md5": msg['image_md5']
            }
        }
        ret = requests.post(url=self.webhook, data=json.dumps(data), headers=self.headers).json()
        self.__log(ret, msg['image_md5'])
