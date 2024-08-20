#!/usr/bin/env python
# -*- coding:utf-8 -*-
# @Date   :2022/1/7
# @Author :p_pyjli
# @File   :elastalert.py
# @Desc   :对接elastalert告警程序，进行告警内容整理并发送

import json
from flask import Blueprint
from flask import request
from lib.logger import log
from core.wechat import Wechat
from core.mail import SendMail
from core.settings import wechat_webhook, mail_to_host, elastalert_c

if elastalert_c.get('wechat_webhook'):
    send_wechat = Wechat(elastalert_c.get('wechat_webhook'))
else:
    send_wechat = Wechat(wechat_webhook)

if elastalert_c.get('mail_to_host'):
    send_mail = SendMail(elastalert_c.get('mail_to_host'))  # 实例化邮件告警实例
else:
    send_mail = SendMail(mail_to_host)

elastalert_opt = Blueprint('elastalert_opt', __name__)


@elastalert_opt.route('/alert/elastalert', methods=['post'])
def elastalert_alert():
    """
    es告警程序，通过在elastalert中rule配置http_post_url进行向该程序发送告警信息，接收到告警信息推送至企业微信
    :return:
    """
    rio_log_desc = {
        "duration": "请求耗时(ms)",
        "errorInfo": "错误信息",
        "orgHost": "HTTP 请求源主机IP",
        "orgPathName": "请求源路径",
        "remoteAddress": "客户端主机IP",
        "sourceName": "规则的appName",
        "statusCode": "状态码",
        "_id": "ES文档唯一ID",
        "_index": "ES索引名称",
        "_score": "ES排序评分",
        "_type": "ES类型名称",
        "host": "目标主机头",
        "matchCost": "匹配规则时长",
        "method": "HTTP 请求方法",
        "path": "目标路径",
        "port": "目标端口",
        "referer": "HTTP 请求的 referer",
        "reqLength": "请求 body 长度",
        "resLength": "响应 body 长度",
        "serverIp": "目标主机的 IP 地址",
        "targetName": "规则的targetName",
        "userAgent": "HTTP 请求的 User - Agent",
        "clientIP": "客户端IP",
        "forwardedFor": "HTTP请求的X - Forward - For",
        "xRioSeq": "来源请求的唯一序列号",
        "targetXRioSeq": "请求目标的唯一序列号",
        "backServerAddress": "后端服务",
        "@timestamp": "时间"
    }
    json_data = json.loads(request.get_data())
    log.info(f"Get elastalert Data: {json_data}")
    try:
        subject = json_data.get("subject")
        alert_message = f'{subject}'
        json_data.pop("subject")
        log.info("Start handle data. ")

        if json_data.get("alert_type") == "rio":
            markdown_message = f'**{alert_message}**\n' + f'> {rio_log_desc.get("@timestamp")}: <font color=\"info\">{json_data.get("@timestamp")}</font>\n' \
                                               f'>{rio_log_desc.get("orgPathName")}: <font color=\"warning\">{json_data.get("orgPathName")}</font>\n' \
                                               f'>{rio_log_desc.get("path")}: <font color=\"warning\">{json_data.get("path")}</font>\n' \
                                               f'>{rio_log_desc.get("duration")}: <font color=\"warning\">{json_data.get("duration")}</font>\n' \
                                               f'>{rio_log_desc.get("statusCode")}: <font color=\"warning\">{json_data.get("statusCode")}</font>\n' \
                                               f'>{rio_log_desc.get("errorInfo")}: <font color=\"warning\">{json_data.get("errorInfo")}</font>'
            html_message = f'<h3>{alert_message}</h3>' + '\n' \
                           + f'<p>{rio_log_desc.get("@timestamp")}: <font color=\"info\">{json_data.get("@timestamp")}</font></p>' \
                             f'<p>{rio_log_desc.get("orgPathName")}: <font color=\"warning\">{json_data.get("orgPathName")}</font></p>' \
                             f'<p>{rio_log_desc.get("path")}: <font color=\"warning\">{json_data.get("path")}</font></p>' \
                             f'<p>{rio_log_desc.get("duration")}: <font color=\"warning\">{json_data.get("duration")}</font></p>' \
                             f'<p>{rio_log_desc.get("statusCode")}: <font color=\"warning\">{json_data.get("statusCode")}</font></p>' \
                             f'<p>{rio_log_desc.get("errorInfo")}: <font color=\"warning\">{json_data.get("errorInfo")}</font></p>'
            log.info("Start send alert message. ")
            if elastalert_c.get('open'):
                if elastalert_c.get('wechat_open'):
                    send_wechat.markdown_user(markdown_message, elastalert_c.get('users'))
                if elastalert_c.get('email_open'):
                    send_mail.send_text_mail(subject, html_message)
            return 'ok'

        elif json_data.get("alert_type") == "public":
            json_data.pop("alert_type")
            markdown_message = f'**{alert_message}**\n'
            html_message = f'<h3>{subject}</h3>'
            for k, v in json_data.items():
                markdown_message += f'>{k}: <font color=\"warning\">{v}</font>\n'
                html_message += f'<p>{k}: <font color=\"warning\">{v}</font></p>'
            if elastalert_c.get('open'):
                if elastalert_c.get('wechat_open'):
                    send_wechat.markdown_user(markdown_message, elastalert_c.get('users'))
                if elastalert_c.get('email_open'):
                    send_mail.send_text_mail(subject, html_message)
            return 'ok'

        else:
            log.error("Alert message type configure error... ")
            return 'failed.'
    except:
        log.error("ES Alert message format failed... ")
        return 'failed.'
