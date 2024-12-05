#!/bin/bash

# 스크립트 파일: build_all_configs.sh

# config 파일 경로 설정
CONFIG_DIR="./config_files"
CONFIG_FILES=(
  "config-8core-default.json"
  "config-8core-1re.json"
  "config-8core-1re-single.json"
  "config-8core-10re.json"
  "config-8core-10re-single.json"
  "config-8core-100re.json"
  "config-8core-100re-single.json"
  "config-8core-1000re.json"
  "config-8core-1000re-single.json"
  "config-8core-10000re.json"
  "config-8core-10000re-single.json"
  "config-8core-100000re.json"
  "config-8core-100000re-single.json"
)

# config 파일을 하나씩 처리
for CONFIG_FILE in "${CONFIG_FILES[@]}"; do
  echo "Processing $CONFIG_FILE..."
  
  # Step 1: config.sh 실행
  ./config.sh "$CONFIG_DIR/$CONFIG_FILE"
  if [ $? -ne 0 ]; then
    echo "Error: config.sh failed for $CONFIG_FILE. Exiting."
    exit 1
  fi

  # Step 2: make 실행
  echo "Running make for $CONFIG_FILE..."
  make
  if [ $? -ne 0 ]; then
    echo "Error: make failed for $CONFIG_FILE. Exiting."
    exit 1
  fi

  echo "Completed $CONFIG_FILE."
  echo "-----------------------------------"
done

# 완료 메시지
echo "All configurations processed successfully!"
