#!/usr/bin/env python
# -*- coding:utf-8 -*-
# @Date   :2022/1/20
# @Author :p_pyjli
# @File   :alertmanager.py
# @Desc   :


import json
import datetime
from jinja2 import Template
from flask import Blueprint
from flask import request
from lib.logger import log
from core.wechat import Wechat
from core.mail import SendMail
from core.settings import wechat_webhook, mail_to_host, prometheus_c

# 这里判断是否独立配置webhook地址，如果有则发送到独立的企业微信群中，如果没有，统一使用gloable全局的地址
if prometheus_c.get('wechat_webhook'):
    send_wechat = Wechat(prometheus_c.get('wechat_webhook'))
else:
    send_wechat = Wechat(wechat_webhook)

# 这里判断是否配置独立收件人，如果有则发送到配置到接收人员邮箱，如果没有，统一发送到gloable全局到收件人
if prometheus_c.get('mail_to_host'):
    send_mail = SendMail(prometheus_c.get('mail_to_host'))  # 实例化邮件告警实例
else:
    send_mail = SendMail(mail_to_host)

alertmanager_opt = Blueprint('alertmanager_opt', __name__)


def get_time_stamp(result):
    """
    用于时间转换，将utc时间转换为本地时间
    @param result: '2022-01-20T09:21:31.669Z'
    @return: 返回本地时间格式
    """
    if len(result) > 24:
        result = result[0:23] + 'Z'
    utct_date1 = datetime.datetime.strptime(result, "%Y-%m-%dT%H:%M:%S.%f%z")
    local_date = utct_date1 + datetime.timedelta(hours=8)  # 加上时区
    local_date_srt = datetime.datetime.strftime(local_date, "%Y-%m-%d %H:%M:%S")
    return local_date_srt


@alertmanager_opt.route('/alert/prom', methods=['post'])
def alertmanager_alert():
    try:
        json_data = json.loads(request.get_data())
        log.info(f"Get alertmanager send Data: {json_data}")

        alert_datas = json_data.get('alerts')
        for alert_data in alert_datas:
            firing_at, recovery_at = '', ''
            if alert_data.get('status') == 'firing':  # 判断状态，获取故障时间
                firing_at = get_time_stamp(alert_data.get('startsAt'))  # 拿到故障时间并格式化
            if alert_data.get('status') == 'resolved':  # 判断状态，获取故障恢复时间
                firing_at = get_time_stamp(alert_data.get('startsAt'))  # 拿到故障时间并格式化
                recovery_at = get_time_stamp(alert_data.get('endsAt'))  # 拿到故障恢复时间并格式化

            subject = prometheus_c.get('subject')  # 获取告警主题

            # 格式化markdown格式告警内容，发送企业微信群使用
            template_str_markdown = """**{{ subject }}**
                                    {% if data.status == "firing" %}>告警名称：<font color="warning">{{ data.labels.alertname }}</font>
                                        >告警实例：<font color="warning">{{ data.labels.instance }}</font>
                                        >告警级别：<font color="warning">{{ data.labels.status }}</font>
                                        >故障时间：<font color="warning">{{ starts_at }}</font>
                                        >当前状态：<font color="warning">{{ data.status }}</font>
                                        >告警描述：<font color="warning">{{ data.annotations.summary }}</font>   
                                        >告警详情：<font color="warning">{{ data.annotations.description }}</font>      
                                    {% elif data.status == "resolved" %}>告警名称：<font color="info">{{ data.labels.alertname }}</font>
                                        >告警实例：<font color="info">{{ data.labels.instance }}</font>
                                        >告警级别：<font color="info">{{ data.labels.status }}</font>
                                        >故障时间：<font color="info">{{ starts_at }}</font>
                                        >当前状态：<font color="info">{{ data.status }}</font>
                                        >告警描述：<font color="info">{{ data.annotations.summary }}</font>   
                                        >告警详情：<font color="info">{{ data.annotations.description }}</font>
                                        >恢复时间：<font color="info">{{ recovery_at }}</font>
                                    {% else %}
                                        >告警实例：<font color="comment">{{ data.labels.instance }}</font>
                                        >告警名称：<font color="comment">{{ data.labels.alertname }}</font>
                                        >告警级别：<font color="comment">{{ data.labels.status }}</font>
                                        >告警时间：<font color="comment">{{ starts_at }}</font>
                                        >当前状态：<font color="comment">{{ data.status }}</font>
                                        >告警描述：<font color="comment">{{ data.annotations.summary }}</font>  
                                        >告警详情：<font color="comment">{{ data.annotations.description }}</font>{% endif %}"""

            # 格式化html格式告警内容，发送邮件时使用
            template_str_html = """<h3>{{ subject }}</h3>
                {% if data.status == "firing" %}<p>告警名称：<font color="warning">{{ data.labels.alertname }}</font></p>
                    <p>告警实例：<font color="warning">{{ data.labels.instance }}</font></p>
                    <p>告警级别：<font color="warning">{{ data.labels.status }}</font></p>
                    <p>故障时间：<font color="warning">{{ starts_at }}</font></p>
                    <p>当前状态：<font color="warning">{{ data.status }}</font></p>
                    <p>告警描述：<font color="warning">{{ data.annotations.summary }}</font></p>
                    <p>告警详情：<font color="warning">{{ data.annotations.description }}</font></p>      
                {% elif data.status == "resolved" %}<p>告警名称：<font color="info">{{ data.labels.alertname }}</font></p>
                    <p>告警实例：<font color="info">{{ data.labels.instance }}</font></p>
                    <p>告警级别：<font color="info">{{ data.labels.status }}</font></p>
                    <p>故障时间：<font color="info">{{ starts_at }}</font></p>
                    <p>当前状态：<font color="info">{{ data.status }}</font></p>
                    <p>告警描述：<font color="info">{{ data.annotations.summary }}</font></p>
                    <p>告警详情：<font color="info">{{ data.annotations.description }}</font></p>
                    <p>恢复时间：<font color="info">{{ recovery_at }}</font></p>
                {% else %}
                    <p>告警实例：<font color="comment">{{ data.labels.instance }}</font></p>
                    <p>告警名称：<font color="comment">{{ data.labels.alertname }}</font></p>
                    <p>告警级别：<font color="comment">{{ data.labels.status }}</font></p>
                    <p>告警时间：<font color="comment">{{ starts_at }}</font></p>
                    <p>当前状态：<font color="comment">{{ data.status }}</font></p>
                    <p>告警描述：<font color="comment">{{ data.annotations.summary }}</font></p>
                    <p>告警详情：<font color="comment">{{ data.annotations.description }}</font>{% endif %}"""

            # 使用jinj2格式化
            markdown_template = Template(template_str_markdown)
            html_template = Template(template_str_html)
            markdown_content = markdown_template.render(data=alert_data, subject=subject, starts_at=firing_at,
                                                        recovery_at=recovery_at)
            html_content = html_template.render(data=alert_data, subject=subject, starts_at=firing_at,
                                                recovery_at=recovery_at)
            # 开始发送告警信息
            if prometheus_c.get('open'):
                if prometheus_c.get('wechat_open'):
                    send_wechat.markdown_user(markdown_content, prometheus_c.get('users'))
                if prometheus_c.get('email_open'):
                    send_mail.send_text_mail(prometheus_c.get('subject'), html_content)
        return 'ok'
    except Exception as e:
        log.error(e)
        log.error("prometheus Alert message format failed... ")
        return 'failed.'

