#!/usr/bin/env python
# -*- coding:utf-8 -*-
# @Date   :2022/1/10
# @Author :p_pyjli
# @File   :tbase.py
# @Desc   :用于腾讯tbase告警对接，发送企业微信以及邮件告警通道


import json
from flask import Blueprint
from flask import request
from lib.logger import log
from core.wechat import Wechat
from core.mail import SendMail
from core.settings import wechat_webhook, mail_to_host, tbase_c

# 这里判断是否独立配置webhook地址，如果有则发送到独立的企业微信群中，如果没有，统一使用gloable全局的地址
if tbase_c.get('wechat_webhook'):
    send_wechat = Wechat(tbase_c.get('wechat_webhook'))
else:
    send_wechat = Wechat(wechat_webhook)

# 这里判断是否配置独立收件人，如果有则发送到配置到接收人员邮箱，如果没有，统一发送到gloable全局到收件人
if tbase_c.get('mail_to_host'):
    send_mail = SendMail(tbase_c.get('mail_to_host'))  # 实例化邮件告警实例
else:
    send_mail = SendMail(mail_to_host)

tbase_opt = Blueprint('tbase_opt', __name__)


@tbase_opt.route('/alert/tbase', methods=['get', 'post'])
def tbase_alert():
    # 获取 tbase 告警发送过来的消息
    json_data = json.loads(request.get_data())
    log.info(f'Get TBase alert message: {json_data}')
    try:
        # content = "cluster: tbase_1_2, node: 10.109.101.23:11022, timestamp: 2021-12-15 12:22:48, Metric: max_conn is abnormal, value: 1910, policy_text: 当指标“当前连接数”的值大于：1500 时告警"
        content = json_data.get('content')
        # ret = ['cluster: tbase_1_2', 'node: 10.109.101.23:11022', 'timestamp: 2021-12-15 12:22:48', 'Metric: max_conn is abnormal', 'value: 1910', 'policy_text: 当指标“当前连接数”的值大于：1500 时告警']
        ret = [x.strip() for x in content.split(",")]
        # new_dic = {'cluster': 'tbase_1_2', 'node': '10.109.101.23:11022', 'timestamp': '2021-12-15 12:22:48', 'Metric': 'max_conn is abnormal', 'value': '1910', 'policy_text': '当指标“当前连接数”的值大于：1500 时告警'}
        new_dic = {i.split(':', 1)[0].strip(): i.split(':', 1)[1].strip() for i in ret}

        # 整理告警内容
        markdown_message = f'**TBase平台告警通知**\n'
        html_message = f'<h3>TBase平台告警通知</h3>' + '\n'
        for k, v in new_dic.items():
            markdown_message += f'{k}: <font color=\"warning\">{v}</font>\n'
            html_message += f'<p>{k}: <font color=\"warning\">{v}</font></p>'

        log.info("Start send TBase alert message. ")
        if tbase_c.get('open'):
            if tbase_c.get('wechat_open'):
                send_wechat.markdown_user(markdown_message, tbase_c.get('users'))
            if tbase_c.get('email_open'):
                send_mail.send_text_mail(tbase_c.get('email_subject'), html_message)

        return 'ok'
    except Exception as e:
        log.error("TBase Alert message format failed... ")
        return 'failed'
