FROM python:3.9-alpine3.14

RUN pip3 install paramiko==2.11.0 openpyxl==2.6.0 tqdm==4.64.0  -i https://mirrors.aliyun.com/pypi/simple/ --no-cache-dir

COPY app  /app

WORKDIR /app

ENTRYPOINT ["python", "hwcheck.py"]
