#!/usr/bin/env python
# -*- coding:utf-8 -*-
# @Date   :2022/1/18
# @Author :p_pyjli
# @File   :tsf.py
# @Desc   :


import json
from flask import Blueprint
from flask import request
from lib.logger import log
from core.wechat import Wechat
from core.mail import SendMail
from core.settings import wechat_webhook, mail_to_host, tsf_c

# 这里判断是否独立配置webhook地址，如果有则发送到独立的企业微信群中，如果没有，统一使用gloable全局的地址
if tsf_c.get('wechat_webhook'):
    send_wechat = Wechat(tsf_c.get('wechat_webhook'))
else:
    send_wechat = Wechat(wechat_webhook)

# 这里判断是否配置独立收件人，如果有则发送到配置到接收人员邮箱，如果没有，统一发送到gloable全局到收件人
if tsf_c.get('mail_to_host'):
    send_mail = SendMail(tsf_c.get('mail_to_host'))  # 实例化邮件告警实例
else:
    send_mail = SendMail(mail_to_host)

tsf_opt = Blueprint('tsf_opt', __name__)


@tsf_opt.route('/alert/tsf', methods=['post'])
def tsf_alert():
    json_data = json.loads(request.get_data())
    log.info(f'Get TSF alert message: {json_data}')
    try:
        title = json_data["title"]  # 接受请求量>10Count
        contents = json_data["content"]
        receivers = json_data['receivers']
        user_list = [x["userName"] for x in receivers]  # 获取用户信息['yinhai']
        alert_time = contents.split(',', 1)[0].split(']')[1]  # 解析告警时间 2022-01-18 15:15:00
        content = contents.split(',', 1)[1]  # namespace-4y4e86vk#102#hsa-cep-bic-local的接受请求量>10Count(策略名:test11, 项目名:)

        # 格式化markdown 告警信息内容
        markdown_message = f'**TSF平台告警通知**\n' \
                           f'>告警主题: <font color=\"info\">{title}</font>\n' \
                           f'>告警时间: <font color=\"info\">{alert_time}</font>\n' \
                           f'>告警详情: <font color=\"warning\">{content}</font>\n' \
                           f'>关注用户: <font color=\"info\">{user_list}</font>\n'
        # 格式化邮件html 告警信息内容
        html_message = f'<h3>TSF平台告警通知</h3>' + '\n' \
                       + f'<p>告警主题: <font color=\"info\">{title}</font></p>' \
                         f'<p>告警时间: <font color=\"info\">{alert_time}</font></p>' \
                         f'<p>告警详情: <font color=\"warning\">{content}</font></p>' \
                         f'<p>关注用户: <font color=\"info\">{user_list}</font></p>'

        # 开始发送告警信息
        log.info("Start send TSF alert message. ")
        if tsf_c.get('open'):
            if tsf_c.get('wechat_open'):
                send_wechat.markdown_user(markdown_message, tsf_c.get('users'))
            if tsf_c.get('email_open'):
                send_mail.send_text_mail(tsf_c.get('email_subject'), html_message)
    except Exception as e:
        log.error("TSF alert message analysis or send failed...")
    return 'ok'
