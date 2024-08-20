#!/usr/bin/env python
# -*- coding:utf-8 -*-
# @Date   :2022/1/10
# @Author :p_pyjli
# @File   :redis.py


import json
import re
import time
from flask import Blueprint
from flask import request
from lib.logger import log
from core.wechat import Wechat
from core.mail import SendMail
from core.settings import wechat_webhook, mail_to_host, credis_c

if credis_c.get('wechat_webhook'):
    send_wechat = Wechat(credis_c.get('wechat_webhook'))
else:
    send_wechat = Wechat(wechat_webhook)

if credis_c.get('mail_to_host'):
    send_mail = SendMail(credis_c.get('mail_to_host'))  # 实例化邮件告警实例
else:
    send_mail = SendMail(mail_to_host)

redis_opt = Blueprint('redis_opt', __name__)


@redis_opt.route('/alert/redis', methods=['get', 'post'])
def redis_alert():
    """
    用于腾讯独立部署版Redis告警消息发送，
    说明：目前告警消息中的（执行错误、驱逐数）暂时没有
    :return:
    """
    alert_msg = {
        "qps": {"name": "总请求-QPS", "unit": "次/秒"},
        "out_flow": {"name": "出流量", "unit": "Mb/min"},
        "in_flow": {"name": "入流量", "unit": "Mb/min"},
        "connections": {"name": "连接数", "unit": "个"},
        "expired_keys_offset": {"name": "key过期数", "unit": "个"},
        "keys": {"name": "key总个数", "unit": "个"},
        "stat_missed_min": {"name": "读请求Miss", "unit": "个"},
        "stat_success_min": {"name": "读请求命中", "unit": "个"},
        "slow_query_min": {"name": "慢查询", "unit": "个"},
        "storage_us": {"name": "内存使用率", "unit": "%"},
        "storage": {"name": "内存使用量", "unit": "Mb"},
        "cpu_us": {"name": "CPU使用率", "unit": "%"}
    }
    symbol = {
        ">": "大于",
        ">=": "大于等于",
        "<": "小于",
        "<=": "大于等于",
        "!=": "不等于",
        "=": "等于",
    }

    json_data = json.loads(request.get_data())
    log.info(f"Get Redis Data: {json_data}")
    prefix = json_data.get('prefix')
    msg = json_data.get('msg')
    emsg_r = re.search(r"] .*", msg, flags=0)
    emsg = emsg_r.group().split(']')[1].strip()  # cpu_us > 1
    emsg_li = emsg.split()  # ['cpu_us', '>', '1']
    try:
        send_ms = alert_msg.get(emsg_li[0]).get('name') + symbol.get(emsg_li[1]) + str(emsg_li[2]) + alert_msg.get(
            emsg_li[0]).get('unit')
    except:
        send_ms = msg
    atime = int(json_data.get('time')) - 10
    ftime = time.strftime('%Y-%m-%d %X', time.localtime(atime))
    try:
        log.info("Start handle data. ")
        appid_r = re.search(r"AppID:\d+]", msg, flags=0)
        appid = appid_r.group().strip(']').split(':')[1]
        nodeid = None
        nodeid_r = re.search(r"NodeID:\d+]", msg)
        if nodeid_r: 
            nodeid = nodeid_r.group().strip(']').split(':')[1]
        emsg_r = re.search(r"] .*", msg, flags=0)
        emsg = emsg_r.group().split(']')[1].strip()
        if nodeid:
            markdown_message = f'**Redis平台告警通知-{prefix}**\n' \
                               f'>实例ID: <font color=\"info\">{appid}</font>\n' \
                               f'>分片ID：<font color=\"info\">{nodeid}</font>\n' \
                               f'>告警时间: <font color=\"info\">{ftime}</font>\n' \
                               f'>告警信息：<font color=\"warning\">{send_ms}</font>\n' \
                               f'>告警详情：<font color=\"warning\">{msg}</font>\n'
            html_message = f'<h3>Redis平台告警通知-{prefix}</h3>' + '\n' \
                           + f'<p>实例ID: <font color=\"info\">{appid}</font></p>' \
                             f'<p>分区ID: <font color=\"info\">{nodeid}</font></p>' \
                             f'<p>告警时间: <font color=\"info\">{ftime}</font></p>' \
                             f'<p>告警信息：<font color=\"warning\">{send_ms}</font></p>' \
                             f'<p>告警详情：<font color=\"warning\">{msg}</font></p>'
        else:
            markdown_message = f'**Redis平台告警通知-{prefix}**\n' \
                               f'>实例ID: <font color=\"info\">{appid}</font>\n' \
                               f'>告警时间: <font color=\"info\">{ftime}</font>\n' \
                               f'>告警信息：<font color=\"warning\">{send_ms}</font>\n' \
                               f'>告警详情：<font color=\"warning\">{msg}</font>\n'
            html_message = f'<h3>Redis平台告警通知-{prefix}</h3>' + '\n' \
                           + f'<p>实例ID: <font color=\"info\">{appid}</font></p>' \
                             f'<p>告警时间: <font color=\"info\">{ftime}</font></p>' \
                             f'<p>告警信息：<font color=\"warning\">{send_ms}</font></p>' \
                             f'<p>告警详情：<font color=\"warning\">{msg}</font></p>'

        log.info("Start send alert message. ")
        if credis_c.get('open'):
            if credis_c.get('wechat_open'):
                send_wechat.markdown_user(markdown_message, credis_c.get('users'))
            if credis_c.get('email_open'):
                send_mail.send_text_mail(credis_c.get('email_subject'), html_message)
        return 'ok'
    except:
        log.error("Redis Alert Handle Failed...")
        return 'failed'

