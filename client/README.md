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

윈도우의 환경 변수 추가를 들어간다.
시스템 변수에 LC_ALL = C.UTF-8 추가


