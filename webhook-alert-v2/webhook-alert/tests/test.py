#!/usr/bin/env python
# -*- coding:utf-8 -*-
# @Date   :2022/1/18
# @Author :p_pyjli
# @File   :test.py
# @Desc   :


import requests
import json


def client(url, body):
    headers = {'Content-Type': 'application/json;charset=utf-8'}
    ret = requests.post(url=url, data=json.dumps(body), headers=headers)
    return ret


rio_body = {"referer": "None",
            "targetName": "10.109.103.149",
            "deviceTicket": "None",
            "orgPathName": "/ebus/mbs_gg_dev/api/publicRetirement/queryRetrShowInfo",
            "orgHost": "192.168.204.31:8080",
            "reqLength": "-1",
            "resLength": "-1",
            "targetXRioSeq": "None",
            "errorInfo": "client request aborted",
            "staffname": "None",
            "source": "20211229.access_8.csv",
            "orgUrl": "/ebus/mbs_gg_dev/api/publicRetirement/queryRetrShowInfo?",
            "deviceId": "None",
            "duration": 10002,
            "authorization": "None",
            "path": "None",
            "host": "None",
            "beat": {"hostname": "host-192-168-204-31", "name": "host-192-168-204-31", "version": "6.6.2"},
            "srvid": "mbs_gg_dev_api_publicretirement_queryretrshowinfo",
            "paasid": "mbs_gg_dev",
            "remoteAddress": "192.168.204.29",
            "ext": {"timeSendUpstream": "1640739280796"},
            "offset": 459903831,
            "method": "POST",
            "methodName": "None",
            "userAgent": "Apache-HttpClient/4.5.9 (Java/1.8.0_252)",
            "xRioSeq": "WIXppFV82J5EH1OR5C3kLIQygIMqPvze",
            "acctype": "pub",
            "requestTime": "2021-12-29T08:54:40.780+08:00",
            "@timestamp": "2021-12-29T08:54:50.782000+08:00",
            "forwardedFor": "192.168.204.241, 192.168.204.29",
            "port": "None",
            "backServerAddress": "None",
            "serverIp": "None",
            "sourceName": "mbs_gg_dev_pub_mbs_gg_dev_api_publicretirement_queryretrshowinfo",
            "fields": {"type": "apigate"},
            "statusCode": "aborted",
            "_id": "fgKuA34BmXbZ4NVm15nM",
            "_index": "sg-access-2021.12.29",
            "_type": "doc",
            "num_hits": 1374,
            "num_matches": 4,
            "subject": "核心区里约网关告警",
            "alert_type": "rio"
            }

elastalert_body = {"referer": "None",
            "targetName": "10.109.103.149",
            "deviceTicket": "None",
            "orgPathName": "/ebus/mbs_gg_dev/api/publicRetirement/queryRetrShowInfo",
            "orgHost": "192.168.204.31:8080",
            "reqLength": "-1",
            "resLength": "-1",
            "targetXRioSeq": "None",
            "errorInfo": "client request aborted",
            "staffname": "None",
            "source": "20211229.access_8.csv",
            "orgUrl": "/ebus/mbs_gg_dev/api/publicRetirement/queryRetrShowInfo?",
            "deviceId": "None",
            "duration": 10002,
            "authorization": "None",
            "path": "None",
            "host": "None",
            "beat": {"hostname": "host-192-168-204-31", "name": "host-192-168-204-31", "version": "6.6.2"},
            "srvid": "mbs_gg_dev_api_publicretirement_queryretrshowinfo",
            "paasid": "mbs_gg_dev",
            "remoteAddress": "192.168.204.29",
            "ext": {"timeSendUpstream": "1640739280796"},
            "offset": 459903831,
            "method": "POST",
            "methodName": "None",
            "userAgent": "Apache-HttpClient/4.5.9 (Java/1.8.0_252)",
            "xRioSeq": "WIXppFV82J5EH1OR5C3kLIQygIMqPvze",
            "acctype": "pub",
            "requestTime": "2021-12-29T08:54:40.780+08:00",
            "@timestamp": "2021-12-29T08:54:50.782000+08:00",
            "forwardedFor": "192.168.204.241, 192.168.204.29",
            "port": "None",
            "backServerAddress": "None",
            "serverIp": "None",
            "sourceName": "mbs_gg_dev_pub_mbs_gg_dev_api_publicretirement_queryretrshowinfo",
            "fields": {"type": "apigate"},
            "statusCode": "aborted",
            "_id": "fgKuA34BmXbZ4NVm15nM",
            "_index": "sg-access-2021.12.29",
            "_type": "doc",
            "num_hits": 1374,
            "num_matches": 4,
            "subject": "核心区里约网关告警",
            "alert_type": "public"
            }

redis_body = {'title': '[TStack Warning]', 'prefix': 'A中心-核心区',
              'msg': 'sys 15003: ip:10.109.105.11 :[AppID:10155] cpu_us > 80', 'time': 1642491915}

tbds_body = {
    'content': '集群tdw中tbds-10-109-100-14机器HDFS服务NAMENODE组件从2021-11-28 03:14:12开始持续DataNode Health Summary告警，组件当前状态：CRITICAL，告警内容：DataNode Health: [Live=47, Stale=1, Dead=0]。 该告警每120分钟发送一次通知，最多发送20次，发送通知时间段：00时-23时。这是第1次发送。请关注',
    'createTime': 1638069281711,
    'recipientContacts': None,
    'subject': 'TBDS服务告警',
    'userName': ['admin'],
    'realName': ['admin'],
    'telPhones': [None],
    'emails': [None]}

tdsql_body = {'ip': '', 'source_time': '1642586160256', 'level': '2', 'alarm_name': '四川医保核心区tdsql告警',
              'alarm_content': '[TDSQL]scyb_coreservice(/tdsqlzk/set_1623914622_135),业务(核心区用友-非分布式1),创建时间(2021-06-17_15:23:42);实例最大CPU利用率大于100%产生告警，后续180分钟内屏蔽告警;异常策略:>阈值(100),当前值:378.0000'}

tbase_body = {'securityCode': '0ec12f1c56d911eaa8af5254003443ae',
               'objType': 'Tbase',
               'objName': 'Tbase',
               'objId': '1479',
               'happenTime': 1639537488,
               'level': 1,
               'content': 'cluster: tbase_1_2, node: 10.109.101.21:11000, timestamp: 2021-12-15 11:03:47, Metric: max_conn is abnormal, value: 1981, policy_text: 当指标“当前连接数”的值大于：1500 时告警'}

tsf_body = {'receivers': [{'phone': None, 'userName': 'yinhai', 'email': None}], 'title': '接受请求量>10Count',
            'content': '[告警]2022-01-19 15:54:00,namespace-9ov69lv3#100000002#hsa-cep-smc-clc-local-5101的接受请求量>10Count(策略名:test11, 项目名:)'}

prometheus_alert_body = {'receiver': 'webhook', 'status': 'firing',
              'alerts': [
                  {'status': 'firing', 'labels': {'alertname': 'NodeDown', 'instance': '192.168.102.133:9100', 'job': 'node', 'status': 'Critical'},
                   'annotations': {'description': '192.168.102.133:9100: 已经宕机超过 5分钟', 'summary': '192.168.102.133:9100: down'},
                   'startsAt': '2022-01-20T09:21:31.669Z',
                   'endsAt': '0001-01-01T00:00:00Z',
                   'generatorURL': 'http://f8d2a7b4758d:9090/graph?g0.expr=up%7Bjob%3D%22node%22%7D+%3D%3D+0&g0.tab=1',
                   'fingerprint': '45aa4ff3ca1a3ce4'}
              ],
              'groupLabels': {'alertname': 'NodeDown'},
              'commonLabels': {'alertname': 'NodeDown', 'instance': '192.168.102.133:9100', 'job': 'node', 'status': 'Critical'},
              'commonAnnotations': {'description': '192.168.102.133:9100: 已经宕机超过 5分钟', 'summary': '192.168.102.133:9100: down'},
              'externalURL': 'http://5bc41386a43f:9093', 'version': '4',
              'groupKey': '{}:{alertname="NodeDown"}', 'truncatedAlerts': 0
              }

prometheus_alert_hf_body = {'receiver': 'webhook', 'status': 'resolved',
             'alerts': [
                 {'status': 'resolved',
                  'labels': {'alertname': 'NodeDown', 'instance': '192.168.102.133:9100', 'job': 'node',
                             'status': 'Critical'},
                  'annotations': {'description': '192.168.102.133:9100: 已经宕机超过 5分钟',
                                  'summary': '192.168.102.133:9100: down'},
                  'startsAt': '2022-01-20T09:21:31.669Z',
                  'endsAt': '2022-01-20T09:36:31.669Z',
                  'generatorURL': 'http://f8d2a7b4758d:9090/graph?g0.expr=up%7Bjob%3D%22node%22%7D+%3D%3D+0&g0.tab=1',
                  'fingerprint': '45aa4ff3ca1a3ce4'}
             ],
             'groupLabels': {'alertname': 'NodeDown'},
             'commonLabels': {'alertname': 'NodeDown', 'instance': '192.168.102.133:9100', 'job': 'node',
                              'status': 'Critical'},
             'commonAnnotations': {'description': '192.168.102.133:9100: 已经宕机超过 5分钟',
                                   'summary': '192.168.102.133:9100: down'},
             'externalURL': 'http://5bc41386a43f:9093', 'version': '4',
             'groupKey': '{}:{alertname="NodeDown"}',
             'truncatedAlerts': 0
             }


# rio = client('http://192.168.101.101:8066/alert/elastalert', rio_body)
elastalert_pub = client('http://192.168.100.31:8066/alert/elastalert', elastalert_body)
# redis = client('http://192.168.101.101:8066/alert/redis', redis_body)
# tbds = client('http://192.168.101.101:8066/alert/tbds', tbds_body)
# tdsql = client('http://192.168.101.101:8066/alert/tdsql', tdsql_body)
# tbase = client('http://192.168.101.101:8066/alert/tbase', tbase_body)
# tsf = client('http://192.168.101.101:8066/alert/tsf', tsf_body)
# prom_alert = client('http://192.168.101.101:8066/alert/prom', prometheus_alert_body)
# prom_huifu_alert = client('http://192.168.101.101:8066/alert/prom', prometheus_alert_hf_body)




# template = Template(template_str)
# content = template.render(prometheus_data=json_data)

# import datetime
# startsAt = '2022-01-20T09:21:31.669Z'
#
# def get_time_stamp(result):
#     utct_date1 = datetime.datetime.strptime(result, "%Y-%m-%dT%H:%M:%S.%f%z")
#     local_date = utct_date1 + datetime.timedelta(hours=8)  # 加上时区
#     local_date_srt = datetime.datetime.strftime(local_date, "%Y-%m-%d %H:%M:%S")
#     return local_date_srt
#
# ret = get_time_stamp(startsAt)
# print(ret)






'''
import datetime
import time
def get_time_stamp(result):
    utct_date1 = datetime.datetime.strptime(result, "%Y-%m-%dT%H:%M:%S.%f%z")#2020-12-01 03:21:57.330000+00:00
    utct_date2 = time.strptime(result, "%Y-%m-%dT%H:%M:%S.%f%z")#time.struct_time(tm_year=2020, tm_mon=12, tm_mday=1, tm_hour=3, tm_min=21, tm_sec=57, tm_wday=1, tm_yday=336, tm_isdst=-1)
    print(utct_date1)
    print(utct_date2)
    local_date = utct_date1 + datetime.timedelta(hours=8)#加上时区
    local_date_srt = datetime.datetime.strftime(local_date, "%Y-%m-%d %H:%M:%S")#2020-12-01 11:21:57.330000
    print(local_date_srt)
    time_array1 = time.mktime(time.strptime(local_date_srt, "%Y-%m-%d %H:%M:%S"))#1606792917.0
    time_array2 = int(time.mktime(utct_date2))#1606764117
    print(time_array1)
    print(time_array2)


if __name__ == '__main__':
    get_time_stamp(startsAt)
'''