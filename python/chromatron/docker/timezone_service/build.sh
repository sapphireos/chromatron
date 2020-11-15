#!/usr/bin/env bash

cp ../../dist/timezone_service .

sudo docker build -t sapphire_timezone_service .
