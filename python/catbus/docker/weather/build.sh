#!/usr/bin/env bash

cp ../../dist/weather .

sudo docker build -t sapphire_weather .
