#!/usr/bin/env python
# -*- coding:utf-8 -*-
# @Date   :2022/1/10
# @Author :p_pyjli
# @File   :tbds.py
# @Desc   :用于腾讯tbds告警对接，发送企业微信以及邮件告警通道


import json
import time
from flask import Blueprint
from flask import request
from lib.logger import log
from core.wechat import Wechat
from core.mail import SendMail
from core.settings import wechat_webhook, mail_to_host, tbds_c

# 这里判断是否独立配置webhook地址，如果有则发送到独立的企业微信群中，如果没有，统一使用gloable全局的地址
if tbds_c.get('wechat_webhook'):
    send_wechat = Wechat(tbds_c.get('wechat_webhook'))
else:
    send_wechat = Wechat(wechat_webhook)

# 这里判断是否配置独立收件人，如果有则发送到配置到接收人员邮箱，如果没有，统一发送到gloable全局到收件人
if tbds_c.get('mail_to_host'):
    send_mail = SendMail(tbds_c.get('mail_to_host'))  # 实例化邮件告警实例
else:
    send_mail = SendMail(mail_to_host)

tbds_opt = Blueprint('tbds_opt', __name__)


@tbds_opt.route('/alert/tbds', methods=['get', 'post'])
def tbds_alert():
    """
    tbds 告警通道，通过在tbds平台配置http告警，配置api接口，通过该接口进行告警消息的推送
    :return:
    """
    # 获取 tbds 告警发送过来的消息
    json_data = json.loads(request.get_data())
    log.info(f'Get TBDS alert message：{json_data}')
    try:
        content = json_data.get('content')  # 获取告警消息中的content内容
        subject = json_data.get('subject')  # 获取告警消息中的主题
        createTime = json_data.get('createTime')  # 获取告警消息发送时间
        # 将时间格式化
        otherStyleTime = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(float(createTime / 1000)))
        # 数据处理完成，开始整理告警内容并告警
        if subject:
            # 格式化markdown 告警信息内容
            markdown_message = f'**TBDS平台告警通知**\n' \
                               f'>告警主题: <font color=\"info\">{subject}</font>\n' \
                               f'>告警时间: <font color=\"info\">{otherStyleTime}</font>\n' \
                               f'>告警详情: <font color=\"warning\">{content}</font>\n'
            # 格式化邮件html 告警信息内容
            html_message = f'<h3>TBDS平台告警通知</h3>' + '\n' \
                           + f'<p>告警主题: <font color=\"info\">{subject}</font></p>' \
                             f'<p>告警时间: <font color=\"info\">{otherStyleTime}</font></p>' \
                             f'<p>告警详情: <font color=\"warning\">{content}</font></p>'
        else:
            markdown_message = f'**TBDS平台告警通知**\n' \
                               f'>告警时间: <font color=\"info\">{otherStyleTime}</font>\n' \
                               f'>告警详情: <font color=\"warning\">{content}</font>\n'
            html_message = f'<h3>TBDS平台告警通知</h3>' + '\n' \
                           + f'<p>告警时间: <font color=\"info\">{otherStyleTime}</font></p>' \
                             f'<p>告警详情: <font color=\"warning\">{content}</font></p>'

        # 开始发送告警信息
        log.info("Start send TBDS alert message. ")
        if tbds_c.get('open'):
            if tbds_c.get('wechat_open'):
                send_wechat.markdown_user(markdown_message, tbds_c.get('users'))
            if tbds_c.get('email_open'):
                send_mail.send_text_mail(tbds_c.get('email_subject'), html_message)
        return 'ok'
    except Exception as e:
        log.error("TBDS alert message send failed...")
        return 'failed'
