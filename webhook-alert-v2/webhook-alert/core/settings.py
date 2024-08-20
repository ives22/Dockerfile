#!/usr/bin/env python
# -*- coding:utf-8 -*-
# @Date   :2022/1/19
# @Author :p_pyjli
# @File   :settings.py
# @Desc   :读取配置yaml配置文件


import os
import yaml
from lib.logger import log

# 配置文件
CONFIG_FILE = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'conf', 'config.yaml')

if not os.path.exists(CONFIG_FILE):
    log.error(f'config file does not exist ! ')
    exit(0)

with open(CONFIG_FILE, encoding='UTF-8') as stream:
    data = yaml.safe_load(stream)

gloable = data.get('gloable', None)
components = data.get('components', None)
if gloable and components:
    wechat_webhook = gloable.get('wechat_webhook', None)
    email_server = gloable.get('email_server', None)
    mail_to_host = gloable.get('mail_to_host', None)
    elastalert_c = components.get('elastalert', None)
    grafana_c = components.get('grafana', None)
    zabbix_c = components.get('zabbix', None)
    prometheus_c = components.get('prometheus', None)
    credis_c = components.get('credis', None)
    tbds_c = components.get('tbds', None)
    tdsql_c = components.get('tdsql', None)
    tbase_c = components.get('tbase', None)
    tsf_c = components.get('tsf', None)
    ssl_license_c = components.get('ssl_license', None)
else:
    log.error('config error ! ')
