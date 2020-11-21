FROM python:3.7.8-buster

USER root

RUN apt-get update
RUN apt-get install -y git

RUN useradd -ms /bin/bash sapphire

USER sapphire

WORKDIR /home/sapphire
RUN git clone https://github.com/sapphireos/chromatron.git

WORKDIR /home/sapphire/chromatron
RUN git checkout dev

WORKDIR /home/sapphire
COPY tools /home/sapphire/.local/share/Chromatron/tools/

