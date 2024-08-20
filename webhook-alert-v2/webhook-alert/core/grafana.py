#!/usr/bin/env python
# -*- coding:utf-8 -*-
# @Date   :2022/1/7
# @Author :p_pyjli
# @File   :grafana.py
# @Desc   :用于grafana告警，发送企业微信以及邮件告警通道


import os
from flask import Blueprint
from flask import request
from urllib.request import urlretrieve
from lib.logger import log
from lib import handleImage
from core.wechat import Wechat
from core.mail import SendMail
from core.settings import wechat_webhook, mail_to_host, grafana_c

# 这里判断是否独立配置webhook地址，如果有则发送到独立的企业微信群中，如果没有，统一使用gloable全局的地址
if grafana_c.get('wechat_webhook'):
    send_wechat = Wechat(grafana_c.get('wechat_webhook'))
else:
    send_wechat = Wechat(wechat_webhook)

# 这里判断是否配置独立收件人，如果有则发送到配置到接收人员邮箱，如果没有，统一发送到gloable全局到收件人
if grafana_c.get('mail_to_host'):
    send_mail = SendMail(grafana_c.get('mail_to_host'))  # 实例化邮件告警实例
else:
    send_mail = SendMail(mail_to_host)

grafana_opt = Blueprint('grafana_opt', __name__)


@grafana_opt.route('/alert/grafana', methods=['post'])
def grafana_alert():
    # 获取监控数据
    log.info("Start get alert message... ")
    all_message = request.json
    log.info(all_message)
    if all_message:
        try:
            # 获取告警title
            title = all_message.get('title')

            # 获取具体告警信息内容
            eval_matches = all_message.get('evalMatches')

            # 获取告警配置的message信息
            message = all_message.get('message')

            # 获取告警信息的ruleUrl，如果grafana.ini不修改url配置项,则拿到的是“http://localhost:3000”, 所以需要根据配置文件中的grafana_url进行拼接处理
            rule_url = all_message.get('ruleUrl')
            url = rule_url.split('/', 3)[-1]
            grafana_url = grafana_c.get('grafana_url')

            if grafana_url.endswith('/'):
                new_rule_url = grafana_url + url
            else:
                new_rule_url = grafana_url + '/' + url

            # 获取告警信息的图片url，如果grafana.ini不修改url配置项,则拿到的是“http://localhost:3000”, 所以需要根据配置文件中的grafana_url进行拼接处理
            image_url = all_message.get('imageUrl')
            url = image_url.split('/', 3)[-1]
            if grafana_url.endswith('/'):
                new_image_url = grafana_url + url
            else:
                new_image_url = grafana_url + '/' + url

            # 调用函数获取图片的md5值和base64数据值
            # log.info(f'Image-----------info {image_info}')
            image_info = handleImage.get_url_image_info(new_image_url)
            # image_info = get_image_info(new_image_url)

            # 循环处理告警信息
            for eval in eval_matches:
                # 获取每条告警消息中的value值
                value = eval.get('value')
                # 获取每条告警消息中的instance
                instance = eval.get('metric')
                # 获取tags
                tags = eval.get('tags')

                if instance:
                    markdown_message = f'**{title}**\n' \
                                       f'>instance：<font color=\"info\">{instance}</font>\n' \
                                       f'>tags：<font color=\"info\">{tags}</font>\n' \
                                       f'>当前值：<font color=\"warning\">{value}</font>\n' \
                                       f'>消息内容：<font color=\"warning\">{message}</font>\n' \
                                       f'>详情：[{new_rule_url}]({new_rule_url})\n' \
                                       f'>告警趋势图如下：'
                    html_message = f'<h3>{title}</h3>' \
                                   f'<p>instance：<font color=\"info\">{instance}</font></p>' \
                                   f'<p>tags：<font color=\"info\">{tags}</font></p>' \
                                   f'<p>当前值：<font color=\"warning\">{value}</font></p>' \
                                   f'<p>消息内容：<font color=\"warning\">{message}</font>/p>' \
                                   f'<p>详情：{new_rule_url}</p>' \
                                   f'<p>告警趋势图如下：</p>'
                else:
                    markdown_message = f'**{title}**\n' \
                                       f'>tags: <font color=\"info\">{tags}</font>\n' \
                                       f'>当前值：<font color=\"warning\">{value}</font>\n' \
                                       f'>消息内容：<font color=\"warning\">{message}</font>\n' \
                                       f'>详情：[{new_rule_url}]({new_rule_url})\n' \
                                       f'>告警趋势图如下：'
                    html_message = f'<h3>{title}</h3>' \
                                   f'<p>tags：<font color=\"info\">{tags}</font></p>' \
                                   f'<p>当前值：<font color=\"warning\">{value}</font></p>' \
                                   f'<p>消息内容：<font color=\"warning\">{message}</font>/p>' \
                                   f'<p>详情：{new_rule_url}</p>' \
                                   f'<p>告警趋势图如下：</p>'

                # 发送告警markdown类型信息
                log.info("Start send alert message. ")
                # 开始发送告警信息
                if grafana_c.get('open'):
                    # 发送企业微信
                    if grafana_c.get('wechat_open'):
                        send_wechat.markdown(markdown_message)
                        send_wechat.image(image_info)
                    # 发送邮件
                    if grafana_c.get('email_open'):
                        try:
                            urlretrieve(new_image_url, './img.png')  # 下载图片
                            send_mail.send_text_and_image_mail(grafana_c.get('email_subject'), html_message,
                                                               './img.png')  # 发送邮件
                            os.remove('./img.png')
                        except:
                            log.error(f'Download {new_image_url} image failed ! ')
                            exit(0)
        except Exception as e:
            log.error(f'Please check grafana configure or grafana version <= 8.1 ! ')
    else:
        log.warning("Alert Data get failed... ")
    return 'ok'
