#!/usr/bin/env python
# -*- coding:utf-8 -*-
# @Date   :2022/1/23
# @Author :p_pyjli
# @File   :ssl_license.py
# @Desc   :用于和 domain ssl license 程序使用，用于进行告警通知

import json
from flask import Blueprint
from flask import request
from lib.logger import log
from core.wechat import Wechat
from core.mail import SendMail
from core.settings import wechat_webhook, mail_to_host, ssl_license_c

# 这里判断是否独立配置webhook地址，如果有则发送到独立的企业微信群中，如果没有，统一使用gloable全局的地址
if ssl_license_c.get('wechat_webhook'):
    send_wechat = Wechat(ssl_license_c.get('wechat_webhook'))
else:
    send_wechat = Wechat(wechat_webhook)

# 这里判断是否配置独立收件人，如果有则发送到配置到接收人员邮箱，如果没有，统一发送到gloable全局到收件人
if ssl_license_c.get('mail_to_host'):
    send_mail = SendMail(ssl_license_c.get('mail_to_host'))  # 实例化邮件告警实例
else:
    send_mail = SendMail(mail_to_host)

license_ssl_opt = Blueprint('license_ssl_opt', __name__)


@license_ssl_opt.route('/alert/license', methods=['post'])
def ssl_license_alert():
    json_data = json.loads(request.get_data())
    log.info(f"Get license and ssl Data: {json_data}")
    license_data = json_data.get('license')
    ssl_data = json_data.get('ssl')

    if len(license_data) != 0 or len(ssl_data) != 0:
        license_ctt, ssl_ctt, mark_license_ctt, mark_ssl_ctt = '', '', '', ''
        if len(license_data) != 0:
            license_ctt = '><font color=\"comment\">**license到期信息：**</font>\n'
            mark_license_ctt = '<h5><font color=\"comment\">license到期信息：</font></h5>\n'
            for i in license_data:
                license_ctt += f'><font color=\"info\">{i.get("describe")}</font>license将于<font color=\"info\">{i.get("expire_date")}</font>到期，还剩<font color=\"warning\">{i.get("expire_time")}</font>天；\n'
                mark_license_ctt += f'<p><font color=\"info\">{i.get("describe")}</font>license将于<font color=\"info\">{i.get("expire_date")}</font>到期，还剩<font color=\"warning\">{i.get("expire_time")}</font>天；</p>'

        if len(ssl_data) != 0:
            ssl_ctt = '><font color=\"comment\">**域名ssl到期信息：**</font>\n'
            mark_ssl_ctt = '<h5><font color=\"comment\">域名ssl到期信息：</font></h5>\n'
            for i in ssl_data:
                ssl_ctt += f'><font color=\"info\">{i.get("domain")}</font> 域名的SSL证书将于<font color=\"info\">{i.get("expire_date")}</font>到期，还剩<font color=\"warning\">{i.get("remaining_time")}</font>天；\n'
                mark_ssl_ctt += f'<p><font color=\"info\">{i.get("domain")}</font> 域名的SSL证书将于<font color=\"info\">{i.get("expire_date")}</font>到期，还剩<font color=\"warning\">{i.get("remaining_time")}</font>天；</p>'

        markdown_content = f'**证书-license到期通知**\n' + license_ctt + ssl_ctt + f'\n><font color=\"warning\">请及时关注，并更新！避免发生不必要的故障！ \(^o^) </font>'
        html_content = f'<h3>证书-license到期通知</h3>\n' + mark_license_ctt + mark_ssl_ctt + f'\n<p><font color=\"warning\">请及时关注，并更新！避免发生不必要的故障！ \(^o^) </font></p>'

        # 开始发送告警信息
        log.info("Start send TSF alert message. ")
        if ssl_license_c.get('open'):
            if ssl_license_c.get('wechat_open'):
                send_wechat.markdown_user(markdown_content, ssl_license_c.get('users'))
            if ssl_license_c.get('email_open'):
                send_mail.send_text_mail(ssl_license_c.get('email_subject'), html_content)
    else:
        log.info('No alarm information ... ')
    return 'ok'

