#!/usr/bin/env bash

sudo docker run -d --restart=always --network=host --name weather sapphire_weather:latest
