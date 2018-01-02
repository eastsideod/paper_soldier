# !/usr/bin/bash

# 테스터용 임시 폴더
mkdir tester_build
cd tester_build && cmake ../ && make -j4

FILE=tester-local
if [ -f $FILE ]; then
  echo "[SYSTEM] Start Testing."
  # TODO(Inkeun): 모든 *-local을 실행시킬 수 있도록 변경한다.
  ./$FILE
else
  echo "[ERROR] Failed to start testing. '$FILE' does not exist."
fi

# 성공하던, 실패하던 테스터를 종료시킨다.
cd ..
rm -rf tester_build

