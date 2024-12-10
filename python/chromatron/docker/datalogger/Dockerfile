FROM ubuntu:24.04

USER root
RUN apt update
RUN apt install -y python3 python3-pip
RUN apt install -y avahi-utils

RUN useradd -ms /bin/bash sapphire

WORKDIR /app

COPY . .

RUN chown -R sapphire /app
RUN chmod 777 /app

# RUN python3 setup.py install
RUN pip3 install . --break-system-packages


USER sapphire

ENTRYPOINT ["python3", "datalogger_main.py"]
