#!/usr/bin/env python
# -*- coding:utf-8 -*-
# @Date   :2022/1/9
# @Author :p_pyjli
# @File   :zabbix.py
# @Desc   :对接zabbix告警，进行告警信息发送，发送至企业微信群以及邮件


import json
from flask import Blueprint
from flask import request
from lib.logger import log
from core.wechat import Wechat
from core.mail import SendMail
from core.settings import wechat_webhook, mail_to_host, zabbix_c

# 这里判断是否独立配置webhook地址，如果有则发送到独立的企业微信群中，如果没有，统一使用gloable全局的地址
if zabbix_c.get('wechat_webhook'):
    send_wechat = Wechat(zabbix_c.get('wechat_webhook'))
else:
    send_wechat = Wechat(wechat_webhook)

# 这里判断是否配置独立收件人，如果有则发送到配置到接收人员邮箱，如果没有，统一发送到gloable全局到收件人
if zabbix_c.get('mail_to_host'):
    send_mail = SendMail(zabbix_c.get('mail_to_host'))  # 实例化邮件告警实例
else:
    send_mail = SendMail(mail_to_host)

zabbix_opt = Blueprint('zabbix_opt', __name__)


@zabbix_opt.route('/alert/zabbix', methods=['post'])
def zabbix_alert():
    """
    对接zabbix告警，发送告警消息推送至企业微信群中
    :return:
    """
    # 获取zabbix 告警发过来的消息，转换为字典格式
    json_data = json.loads(request.get_data())

    try:
        # 获取具体告警内容
        text_data = json_data.get('content')
        text_data_list = text_data.split('split_data')
        message = text_data_list[0]
        # 格式化告警信息
        message_list = message.split('\n')
        title = message_list.pop(0)
        if message_list[-1] == '':
            message_list.pop(-1)
        markdown_message = f"**{title}**\n"
        html_message = f'<h3>{title}</h3>' + '\n'
        for i in message_list:
            if "等级" in i.split('：', 1)[0] or "详情" in i.split('：', 1)[0] or "状态" in i.split('：', 1)[0]:
                markdown_message += f">{i.split('：', 1)[0]}: <font color=\"warning\">{i.split('：', 1)[1]}</font>\n"
                html_message += f"<p>{i.split('：', 1)[0]}: <font color=\"warning\">{i.split('：', 1)[1]}</font></p>"
            else:
                markdown_message += f">{i.split('：', 1)[0]}: <font color=\"info\">{i.split('：', 1)[1]}</font>\n"
                html_message += f"<p>{i.split('：', 1)[0]}: <font color=\"info\">{i.split('：', 1)[1]}</font></p>"
        users = text_data_list[1].strip()  # 获取所用人员，去除字符串两边的空格

        if len(users) == 0 and message:
            log.info(f'Get Zabbix alert message：{markdown_message}')
            # 发送告警信息
            log.info("Start send alert message. ")
            if zabbix_c.get('open'):
                if zabbix_c.get('wechat_open'):
                    send_wechat.markdown(markdown_message)
                if zabbix_c.get('email_open'):
                    send_mail.send_text_mail(zabbix_c.get('email_subject'), html_message)

        elif ',' in users:
            user_list = [user for user in users.split(',')]  # 如果有多个用户，以逗号切割，并去重两边的空格
            if zabbix_c.get('open'):
                if zabbix_c.get('wechat_open'):
                    send_wechat.markdown_user(markdown_message, user_list)
                if zabbix_c.get('email_open'):
                    send_mail.send_text_mail(zabbix_c.get('email_subject'), html_message)
        else:
            user_list = []
            user_list.append(users)
            if zabbix_c.get('open'):
                if zabbix_c.get('wechat_open'):
                    send_wechat.markdown_user(markdown_message, user_list)
                if zabbix_c.get('email_open'):
                    send_mail.send_text_mail(zabbix_c.get('email_subject'), html_message)
        return 'ok'
    except:
        log.info("Please check zabbix config... ")
        return 'failed.'
