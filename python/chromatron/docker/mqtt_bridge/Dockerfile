FROM ubuntu:20.04

USER root
RUN apt update
RUN apt install -y python3 python3-pip

RUN useradd -ms /bin/bash sapphire

WORKDIR /app

COPY . .

RUN chown -R sapphire /app
RUN chmod 777 /app

RUN python3 setup.py install

USER sapphire

ENTRYPOINT ["python3", "mqtt_main.py"]
