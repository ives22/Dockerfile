#!/usr/bin/env python
# -*- coding:utf-8 -*-
# @Date   :2022/1/10
# @Author :p_pyjli
# @File   :tdsql.py
# @Desc   :用于腾讯tdsql告警对接，发送企业微信以及邮件告警通道


import json
import time
from flask import Blueprint
from flask import request
from lib.logger import log
from core.wechat import Wechat
from core.mail import SendMail
from core.settings import wechat_webhook, mail_to_host, tdsql_c

# 这里判断是否独立配置webhook地址，如果有则发送到独立的企业微信群中，如果没有，统一使用gloable全局的地址
if tdsql_c.get('wechat_webhook'):
    send_wechat = Wechat(tdsql_c.get('wechat_webhook'))
else:
    send_wechat = Wechat(wechat_webhook)

# 这里判断是否配置独立收件人，如果有则发送到配置到接收人员邮箱，如果没有，统一发送到gloable全局到收件人
if tdsql_c.get('mail_to_host'):
    send_mail = SendMail(tdsql_c.get('mail_to_host'))  # 实例化邮件告警实例
else:
    send_mail = SendMail(mail_to_host)

tdsql_opt = Blueprint('tdsql_opt', __name__)


@tdsql_opt.route('/alert/tdsql', methods=['get', 'post'])
def tdsql_alert():
    """
    TDSQL 告警通道，通过tdsql自带的告警脚本对接告警消息推送
    :return:
    """
    # /data/application/tdsql_analysis/bin
    # http://10.109.93.50:8066/alert/tdsql
    # 获取 tdsql 告警发送过来的消息
    json_data = json.loads(request.get_data())
    log.info(f'Get TDSQL alert message：{json_data}')
    try:
        log.info("Format tdsql alert message.")
        alert_ip = json_data.get('ip')  # 获取告警IP地址
        source_time = int(json_data.get('source_time'))  # 获取告警时间
        alert_time = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(float(source_time / 1000)))  # 格式化告警时间
        alarm_name = json_data.get('alarm_name')  # 获取告警title
        alarm_content = json_data.get('alarm_content')  # 获取告警详情内容
        # 整理告警内容
        # 格式化markdown 告警信息内容
        markdown_message = f'**{alarm_name}**\n' \
                           f'>告警对象: <font color=\"info\">{alert_ip}</font>\n' \
                           f'>告警时间: <font color=\"info\">{alert_time}</font>\n' \
                           f'>告警详情：<font color=\"warning\">{alarm_content}</font>'
        # 格式化邮件html 告警信息内容
        html_message = f'<h3>{alarm_name}</h3>' + '\n' \
                       + f'<p>告警对象: <font color=\"info\">{alert_ip}</font></p>' \
                         f'<p>告警时间: <font color=\"info\">{alert_time}</font></p>' \
                         f'<p>告警详情：<font color=\"warning\">{alarm_content}</font></p>'

        # 发送告警markdown类型消息，并@指定的运维人员接收告警消息
        log.info("Start send TDSQL alert message. ")
        if tdsql_c.get('open'):
            if tdsql_c.get('wechat_open'):
                send_wechat.markdown_user(markdown_message, tdsql_c.get('users'))
            if tdsql_c.get('email_open'):
                send_mail.send_text_mail(tdsql_c.get('email_subject'), html_message)
        return 'ok'
    except:
        log.error("TDSQL Alert message format failed... ")
        return 'failed.'
