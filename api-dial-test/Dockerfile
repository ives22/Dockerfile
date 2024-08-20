FROM docker.io/python:3.9-alpine3.15

COPY dial_test /dial_test

COPY requirements.txt /tmp/requirements.txt

RUN pip install --no-cache-dir -r /tmp/requirements.txt -i https://mirrors.aliyun.com/pypi/simple/ && rm -f /tmp/requirements.txt

WORKDIR /dial_test

ENTRYPOINT ["python", "ApiDialTest.py"]
