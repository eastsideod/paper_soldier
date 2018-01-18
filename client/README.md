paper solider client development readme


윈도우 Git 한글 설정법.


설정이 필요한것.

Git의 		UTF-8
Window CMD 	UTF-8



Window CMD UTF-8 설정법

방법 1.

cmd 창을 열고 chcp 65001 (UTF-8) 입력
cmd 창에 active xxx 가 뜨고 언어 설정이 65001로 변경됨

cmd 창에서 오른쪽 클릭수 속성을 보면 65001이 설정된 것을 확인 가함.

방법 2.

regedit을 켜고 (레지스트리 편집기)

HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Command에
문자 열값 생성

파일이름은 Autorun으로 설정 (자동으로 실행되는 파일명 같음.)
파일을 열고 값을 chcp 65001 설정

cmd를 열때마다 언어 설정이 utf-8로 적용






Git의 utf-8 설정

Git CMD를 열고 

git config --global i18n.commitEncoding utf-8

git config --global --add i18n.logOutputEncoding utf-8 또는git config --global i18n.logOutputEncoding utf-8

입력하면 적용완료

설정 값을 확인하려면 명령어 맨뒤의 utf-8을 빼면 현재 설정된 언어정보를 보여준다.

