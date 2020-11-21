#!/usr/bin/env bash

cp ../../dist/logserver .

sudo docker build -t sapphire_logserver .
