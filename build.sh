#!/bin/bash
esphome_version=dev

if [ -z "$2" ]; then
    echo "Usage: ./build.sh <esphome_version> <yaml_file>"
    exit 1
fi
yaml_file=$2


docker run --rm -it\
  -v "$(pwd)":"/config" \
  -v "$HOME:$HOME" \
  -e HOME \
  --user $(id -u):$(id -g) \
  esphome/esphome:$esphome_version compile $yaml_file