#!/usr/bin/env python
# -*- coding:utf-8 -*-
# @Date   :2021/12/30
# @Author :p_pyjli
# @File   :main.py
# @Desc   :主程序


from core.index import index_opt
from core.elastalert import elastalert_opt
from core.grafana import grafana_opt
from core.zabbix import zabbix_opt
from core.alertmanager import alertmanager_opt
from core.redis import redis_opt
from core.tbds import tbds_opt
from core.tdsql import tdsql_opt
from core.tbase import tbase_opt
from core.tsf import tsf_opt
from core.ssl_license import license_ssl_opt
from flask import Flask


def start():
    app = Flask(__name__)
    app.register_blueprint(index_opt)
    app.register_blueprint(elastalert_opt)
    app.register_blueprint(grafana_opt)
    app.register_blueprint(zabbix_opt)
    app.register_blueprint(alertmanager_opt)
    app.register_blueprint(redis_opt)
    app.register_blueprint(tbds_opt)
    app.register_blueprint(tdsql_opt)
    app.register_blueprint(tbase_opt)
    app.register_blueprint(tsf_opt)
    app.register_blueprint(license_ssl_opt)
    app.run(host="0.0.0.0", port=8066, debug=True)

