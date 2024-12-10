FROM ubuntu:24.04

# needed to install avahi without a prompt
ARG DEBIAN_FRONTEND=noninteractive

USER root
RUN apt update
RUN apt install -y python3 python3-pip 

# needed for mDNS to work
RUN apt install -y avahi-utils 

RUN useradd -ms /bin/bash sapphire

WORKDIR /app

COPY . .

RUN chown -R sapphire /app
RUN chmod 777 /app

# RUN python3 setup.py install
RUN pip3 install . --break-system-packages

USER sapphire

ENTRYPOINT ["python3", "mqtt_main.py"]
