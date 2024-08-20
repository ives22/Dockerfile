#!/usr/bin/env python
# -*- coding:utf-8 -*-
# @Date   :2022/4/2
# @Author :p_pyjli
# @File   :ApiDialTest.py
# @Desc   :


import re
import json
import smtplib
import yaml
import logging
import requests
import time
import datetime
import random
import string
import hashlib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from concurrent.futures import ThreadPoolExecutor
from tencentcloud.common import credential
from tencentcloud.common.profile.client_profile import ClientProfile
from tencentcloud.common.profile.http_profile import HttpProfile
from tencentcloud.common.exception.tencent_cloud_sdk_exception import TencentCloudSDKException
from tencentcloud.sms.v20210111 import sms_client, models

# 加载配置文件
conf_file = "config.yaml"

# 日志
logging.basicConfig(level=logging.INFO,
                    format='%(asctime)s - %(filename)s[line:%(lineno)d] - %(levelname)s: %(message)s')


# 读取配置文件
def read_config(conf_file=conf_file):
    try:
        with open(conf_file, encoding='UTF-8') as yaml_file:
            conf_list = yaml.safe_load(yaml_file)
        return conf_list['elasticsearch'], conf_list['dial'], conf_list['alert'],
    except Exception as e:
        logging.error(f"read conf_list error: {e}")
        raise


# 获取配置
es, dial_conf, alert_conf = read_config(conf_file)


# 调用es
def request_elasticsearch(host=es['host'], port=es['port'], method="GET", path="/",
                          user=es['user'], password=es['password'], body=""):
    auth = (user, password)
    url = f'http://{host}:{port}{path}'
    data = body
    headers = {
        "Content-Type": "application/json"
    }
    r = requests.request(method=method, url=url, data=data, auth=auth, headers=headers)
    return r.status_code, r.text


# 创建索引
def create_index(index_perfix=es['index_prefix']):
    today = time.strftime("%Y-%m", time.localtime(time.time()))
    index_name = f'{index_perfix}@{today}'
    # 创建索引模板
    template = {
        "settings": {
            "number_of_shards": 3,
            "number_of_replicas": 1
        },
        "mappings": {
            "properties": {
                "api_name": {
                    "type": "keyword",
                    "ignore_above": 256
                },
                "api_url": {
                    "type": "text"
                },
                "response_code": {
                    "type": "keyword",
                    "ignore_above": 256
                },
                "response_content": {
                    "type": "text"
                },
                "response_time": {
                    "type": "float"
                },
                "check_type": {
                    "type": "keyword"
                },
                "check_content": {
                    "type": "keyword"
                },
                "error_message": {
                    "type": "text"
                },
                "status": {
                    "type": "keyword",
                    "ignore_above": 256
                },
                "status_code": {
                    "type": "integer",
                },
                "timestamp": {
                    "type": "date"
                }
            }
        }
    }
    # 检查索引是否存在
    status, text = request_elasticsearch(path=f"/{index_name}")
    if status == 404:
        c_status, c_text = request_elasticsearch(path=f"/{index_name}", body=json.dumps(template), method="PUT")
        if c_status == 200:
            logging.info(f"Create index {index_name} success! ")
            return index_name
        else:
            logging.error(f"Create index {index_name} fail! {c_text}")
    elif status == 200:
        logging.warning(f"index {index_name} is exist! ")
        return index_name
    else:
        logging.error(f"query index error: {text}")


# 读取拨测接口
def read_api_list(config_file=dial_conf['api_conf_file']):
    try:
        with open(config_file, encoding='UTF-8') as yaml_file:
            api_list = yaml.safe_load(yaml_file)
        return api_list
    except Exception as e:
        logging.error(f"read api_list error: {e}")
        raise


# 里约网关认证信息
def rio_headers(rio_paas_id, rio_paas_token):
    paas_id = rio_paas_id
    paas_token = rio_paas_token
    timestamp = int(time.time())
    nonce = ''.join(random.sample(string.ascii_letters + string.digits, 11))
    signature = hashlib.sha256((str(timestamp) + str(paas_token) + nonce + str(timestamp)).encode()).hexdigest()
    header = {
        "x-tif-paasid": paas_id,
        "x-tif-timestamp": str(timestamp),
        "x-tif-signature": signature,
        "x-tif-nonce": nonce
    }
    return header


# 单个接口拨测
def dial_one_api(api_info, dial_conf=dial_conf, alert_conf=alert_conf):
    api_name = api_info["api_name"]
    url = api_info["api_url"]
    body_type = api_info["body_type"]
    data = api_info["body"]
    headers = api_info["headers"]

    if api_info["type"] == "rio":
        headers.update(rio_headers(rio_paas_id=api_info["rio_paas_id"], rio_paas_token=api_info["rio_paas_token"]))
    method = api_info["method"]
    time1 = time.time()
    try:
        # 首先判断接口请求时body类型为json格式，便走发送json格式的消息方法
        if body_type == "json":
            r = requests.request(method=method, data=data.encode(), url=url, headers=headers,
                                 timeout=dial_conf['timeout'])
        # 如果接口请求时body类型为form-data类型，便走发送fome-data格式的消息方法
        elif body_type == "form_data":
            r = requests.request(method='POST', url=url, files=eval(data), headers=headers,
                                 timeout=dial_conf['timeout'])
        else:
            logging.error(f"api body type configure error!  api_name: {api_name}，{data}")

        response_time = time.time() - time1
        # 校验返回
        check_type = api_info["check_type"]
        check_content = api_info["check_content"]
        check_reverse = api_info["check_reverse"]
        if check_type == "code" and r.status_code == check_content:
            status = "success"
            code = 0
        elif check_type == "content":
            pattern = re.compile(check_content)
            a = pattern.findall(r.text)
            if check_reverse:
                if len(a) > 0:
                    status = "failure"
                    code = 1
                else:
                    status = "success"
                    code = 0
            else:
                if len(a) > 0:
                    status = "success"
                    code = 0
                else:
                    status = "failure"
                    code = 1
        else:
            status = "failure"
            code = 1
        return_data = {
            "api_name": api_info["api_name"],
            "api_url": api_info["api_url"],
            "response_code": r.status_code,
            "check_type": api_info["check_type"],
            "check_content": api_info["check_content"],
            "response_content": r.text,
            "response_time": response_time,
            "status": status,
            "status_code": code,
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S.000Z", time.localtime(time.time() - (60 * 60 * 8)))
        }
        # 告警配置
        if code == 1:
            logging.error(f"API {api_info['api_name']} dial fail!")
            if alert_conf['start']:
                if alert_conf['wechat']:
                    send_alert_wechat(api_info["api_name"], api_info["api_url"], r.text)
                if alert_conf['email']:
                    send_alert_email(api_info["api_name"], api_info["api_url"], r.text)
                if alert_conf['sms']:
                    send_alert_sms(api_info["api_name"], api_info["api_url"], r.text)
        return return_data
    except Exception as e:
        logging.error(f"dial error!  api_name: {api_name} api_url: {url} errormessage: {e}")
        # 告警配置
        if alert_conf['start']:
            if alert_conf['wechat']:
                send_alert_wechat(api_info["api_name"], api_info["api_url"], str(e))
            if alert_conf['sms']:
                send_alert_sms(api_info["api_name"], api_info["api_url"], r.text)
        return {
            "api_name": api_info["api_name"],
            "api_url": api_info["api_url"],
            "response_time": time.time() - time1,
            "error_message": str(e),
            "status": "failure",
            "status_code": 1,
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S.000Z", time.localtime(time.time() - (60 * 60 * 8)))
        }


# 并发拨测所有api
def dial_all(api_list, dial_conf=dial_conf):
    # 计算等待时间
    wait_time = dial_conf['timeout'] * (len(api_list) / dial_conf['workers'] + 1) + 1
    logging.info(f'max wait time: {wait_time} s')
    with ThreadPoolExecutor(max_workers=dial_conf['workers']) as executor:
        future_list = [executor.submit(dial_one_api, api_info) for api_info in api_list]
        return [x.result(timeout=wait_time) for x in future_list]


# 推送结果到es
def insert_index(result_list):
    index_name = create_index()
    request_body = ''
    for doc in result_list:
        row = json.dumps(doc)
        request_body += '{"index":{"_type":"_doc"}}\n'
        request_body += f"{row}\n"
    status, text = request_elasticsearch(path=f"/{index_name}/_bulk", body=request_body, method="POST")

    if status == 200:
        res = json.loads(text)
        errors = res["errors"]
        logging.info(f"push elasticsearch success! code: {status}, docs: {len(result_list)}, errors: {errors}")
        for item in res["items"]:
            logging.info(f"push doc: {str(item)}")
    else:
        logging.info(f"push elasticsearch error! code: {status}, {text}")


# 微信告警
def send_alert_wechat(api_name, api_url, error_message, alert_conf=alert_conf):
    alert_time = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    content = "## 接口拨测告警通知！！！\n" \
              ">#### 告警时间：<font color=\"warning\">%s</font>\n" \
              "#### 接口名称：<font color=\"warning\">%s</font>\n" \
              "#### 接口地址：<font color=\"comment\">%s</font>\n" \
              "#### 错误信息：<font color=\"comment\">%s</font>\n" % (alert_time, api_name, api_url, error_message)
    try:
        headers = {
            "Content-Type": "application/json"
        }
        request_body = {
            "msgtype": "markdown",
            "markdown": {
                "content": content
            }
        }
        response_data = requests.request(method="POST", data=json.dumps(request_body),
                                         url=alert_conf['wechat_url'], headers=headers, timeout=5)
        if response_data.status_code == 200:
            error_code = json.loads(response_data.text).get("errcode")
            if error_code == 0:
                logging.info("Send wechat alert message success.")
            else:
                logging.error("Send wechat alert message faild:%s" % response_data.text)
        else:
            logging.error("Send wechat alert message faild:%s" % response_data.status_code)
    except Exception as e:
        logging.error('Send wechat alert message faild:%s' % e)
        exit(1)


def send_alert_email(api_name, api_url, error_message, alert_conf=alert_conf):
    alert_time = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')

    host = alert_conf["email_host"]
    port = alert_conf["email_port"]
    form_addr = alert_conf["email_from_addr"]
    form_auth = alert_conf["email_auth_pass"]
    to_hosts = alert_conf["email_to_hosts"]
    smtp_server = ''
    if port == 25:
        smtp_server = smtplib.SMTP(host=host, port=port)
    elif port == 465:
        smtp_server = smtplib.SMTP_SSL(host=host, port=port)
    else:
        logging.error(f'Email server port configure error !')
        exit(1)

    # 格式化邮件html 告警信息内容
    html_message = f'<h3>接口拨测告警通知，请及时关注！</h3>' + '\n' \
                       + f'<p>告警时间: <font color=\"warning\">{alert_time}</font></p>' \
                         f'<p>接口名称：<font color=\"warning\">{api_name}</font></p>' \
                         f'<p>接口地址：<font color=\"warning\">{api_url}</font></p>' \
                         f'<p>错误信息：<font color=\"warning\">{error_message}</font></p>'

    # 构建邮件信息体
    mailMsg = MIMEMultipart()
    mailMsg['Subject'] = "接口异常告警通知"
    mailMsg['From'] = form_addr
    mailMsg['To'] = ','.join(to_hosts)
    mailMsg.attach(MIMEText(html_message, 'html', 'utf-8'))
    # 登陆邮箱，进行邮件信息发送
    try:
        smtp_server.login(user=form_addr, password=form_auth)
        smtp_server.sendmail(from_addr=form_addr, to_addrs=to_hosts, msg=mailMsg.as_string())
        logging.info("Send email alert message success.")
    except Exception as e:
        logging.error('Send email alert message faild:%s' % e)
        exit(1)

# 短信告警
def send_alert_sms(api_name, api_url, error_message, alert_conf=alert_conf):
    try:
        cred = credential.Credential(alert_conf['sms_secret_id'], alert_conf['sms_secret_key'])
        httpProfile = HttpProfile()
        httpProfile.endpoint = "sms.tencentcloudapi.com"
        clientProfile = ClientProfile()
        clientProfile.httpProfile = httpProfile
        client = sms_client.SmsClient(cred, alert_conf['sms_region'], clientProfile)
        req = models.SendSmsRequest()
        content = "服务拨测失败告警，接口名称：%s，接口地址：%s，错误信息：%s" % (api_name, api_url, error_message)
        # content = "服务拨测失败告警，接口名称：%s，接口地址：%s，错误信息：" %(api_name, api_url)
        params = {
            "PhoneNumberSet": alert_conf['sms_phone_numbers'],
            "SmsSdkAppId": alert_conf['sms_sdk_app_id'],
            "TemplateId": alert_conf['sms_template_id'],
            "SignName": alert_conf['sms_sign_name'],
            "TemplateParamSet": [content]
        }
        req.from_json_string(json.dumps(params))
        # 检查结果
        resp = client.SendSms(req)
        code = resp.SendStatusSet[0].Code
        if code != "Ok":
            logging.error(
                "Send SMS alert failed, {'Code': '%s', 'Message': '%s'}" % (code, resp.SendStatusSet[0].Message))
        else:
            logging.info("Send SMS alert success.")
    except TencentCloudSDKException as err:
        logging.error("发送短信时错误: %s" % err)
        exit(0)


def main():
    # 获取拨测接口列表
    logging.info("start read api list .....")
    api_list = read_api_list()
    # 并发拨测接口
    logging.info("start dial api list .....")
    result = dial_all(api_list=api_list["api"])
    # 插入拨测结果到elasticsearch
    logging.info("start push dial result .....")
    insert_index(result_list=result)


if __name__ == "__main__":
    while True:
        main()
        time.sleep(60)

