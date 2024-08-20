FROM docker.io/python:3.9-alpine3.15

#COPY requirements.txt conf.yaml alert.py /root/

COPY webhook-alert /webhook-alert/

RUN pip install --no-cache-dir -r /webhook-alert/requirements.txt -i https://mirrors.aliyun.com/pypi/simple/

WORKDIR /webhook-alert/bin

ENTRYPOINT ["python", "run.py"]
