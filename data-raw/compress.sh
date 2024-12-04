#!/bin/bash

cd data-raw
for file in *; do
  if [ "$file" != "compress.sh" ]; then
    gzip -c "$file" > "../data/$file.gz"
  fi
done

cd ..