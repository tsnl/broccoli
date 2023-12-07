#!/usr/bin/env bash

for ZIP_PATH in res/model/3rdparty/kenney/*.zip; do
  unzip -o "$ZIP_PATH" -d "${ZIP_PATH%.zip}" || exit 1
done
