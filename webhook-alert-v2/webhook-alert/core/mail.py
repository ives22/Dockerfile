#!/usr/bin/env python
# -*- coding:utf-8 -*-
# @Date   :2021/12/30
# @Author :p_pyjli
# @File   :email.py
# @Desc   :发送邮件


# 引入相应的模块
import smtplib
import os
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from email.mime.multipart import MIMEBase
from email import encoders
from lib.logger import log
from core.settings import email_server


class SendMail(object):
    def __init__(self, mail_to_host):
        self.host = email_server.get('host')
        self.port = email_server.get('port')
        self.fromAddress = email_server.get('fromAddress')
        self.authPass = email_server.get('authPass')
        self.toAddress = mail_to_host

    def __login(self):
        try:
            if self.port == 25:
                smtpObj = smtplib.SMTP(self.host, self.port)    # 连接smtp服务器
            else:
                smtpObj = smtplib.SMTP_SSL(self.host, self.port)    # 连接smtp服务器
            log.info(f'{self.host} Connection success... ')
            try:
                smtpObj.login(self.fromAddress, self.authPass)  # 登录邮箱
                log.info(f'{self.fromAddress} Email login success...')
                return smtpObj
            except Exception as e:
                log.error(f'{self.fromAddress} Email login failed, Please check your account and password ！')
                exit(0)
        except Exception as e:
            log.error(f'{self.host} Connection failed, Please check smtp server configure ! ')
            exit(0)

    def __send_email(self, mail_msg_content):
        """
        发送邮件的函数
        @param mail_msg_content: 邮件内容结构体
        @return:
        """
        server = self.__login()
        server.sendmail(from_addr=self.fromAddress, to_addrs=self.toAddress, msg=mail_msg_content)
        server.quit()

    def send_text_mail(self, subject, content_msg):
        """
        发送纯文本邮件的结构体
        @param subject: 邮件主题
        @param content_msg: 邮件正文内容
        @return:
        """
        # 邮件对象
        mailMsg = MIMEMultipart()
        mailMsg['Subject'] = subject
        mailMsg['From'] = self.fromAddress
        mailMsg['To'] = ','.join(self.toAddress)
        # 邮件正文为MIMEText
        mailMsg.attach(MIMEText(content_msg, 'html', 'utf-8'))

        # 调用发送邮件的函数，进行邮件发送
        self.__send_email(mailMsg.as_string())
        log.info(f'Text email {subject} send success... ')

    def send_text_and_image_mail(self, subject, content_msg, image_path):
        """
        发送文本和图片的邮件，图片嵌套在邮件正文汇总
        @param subject: 邮件主题
        @param content_msg: 邮件正文内容
        @param image_path: 图片路径
        @return:
        """
        if os.path.exists(image_path):
            mailMsg = MIMEMultipart()
            mailMsg['Subject'] = subject
            mailMsg['From'] = self.fromAddress
            mailMsg['To'] = ','.join(self.toAddress)
            # 加载图片并引入到邮件内容中来
            mailMsg.attach(MIMEText('<html><body><h1>%s</h1>' % (content_msg) + '<p><img src="cid:0"></p>' + '</body></html>', 'html', 'utf-8'))
            with open(image_path, 'rb') as file:
                # 设置附件的MIME和文件名，这里是png类型
                mime = MIMEBase('image', 'png', filename='image.png')
                # 加上必要的头信息
                mime.add_header('Content-Disposition', 'attachment', filename='image.png')
                mime.add_header('Content-ID', '<0>')
                mime.add_header('X-Attachment-Id', '0')
                # 把附件的内容读进来
                mime.set_payload(file.read())
                # 用Base64编码
                encoders.encode_base64(mime)
                # 添加到MIMEMultipart
                mailMsg.attach(mime)

            # 调用发送邮件的函数，进行邮件发送
            self.__send_email(mailMsg.as_string())
            log.info(f'Text and image email {subject} send success... ')
        else:
            log.error(f'Image path {image_path} does not exist ! ')
            exit(0)

    def send_enclosure_mail(self, subject, content_msg, enclosure_path, enclosure_name):
        """
        发送附件邮件
        @param subject: 邮件主题
        @param content_msg: 邮件正文内容
        @param enclosure_path: 附件的路径
        @param enclosure_name: 发送邮件时附件的名称
        @return:
        """
        if os.path.exists(enclosure_path):
            mailMsg = MIMEMultipart()
            mailMsg['Subject'] = subject
            mailMsg['From'] = self.fromAddress
            mailMsg['To'] = ','.join(self.toAddress)
            # 添加正文信息
            mailMsg.attach(MIMEText(content_msg, 'html', 'utf-8'))
            # 添加附件文件
            enclosure_msg = MIMEText(open(enclosure_path, 'rb').read(), 'base64', 'utf-8')
            enclosure_msg.add_header('Content-Disposition', 'attachment', filename=('utf-8', '', enclosure_name))
            mailMsg.attach(enclosure_msg)

            # 调用发送邮件的函数，进行邮件发送
            self.__send_email(mailMsg.as_string())
            log.info(f'Enclosure email {subject} send success... ')
        else:
            log.error(f'{enclosure_path} does not exist ! ')
            exit(0)
