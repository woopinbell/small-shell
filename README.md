# small-shell

`small-shell`은 명령줄을 읽고 해석해 실행하는 과정을 C로 구현하며
셸의 데이터 구조와 프로세스 수명을 학습하기 위한 프로젝트다.

## 목표

- C99와 POSIX 인터페이스를 사용한다.
- tokenizer, parser, expansion 단계를 명확히 분리한다.
- builtin과 외부 명령의 실행 경계를 구분한다.
- pipeline, redirection, heredoc의 자원 소유권을 추적한다.
- 오류를 호출자에게 전달하고 부분 결과를 정리한다.

완성된 프로그램은 표준 입력에서 한 줄씩 읽어 제한된 셸 문법을 실행하는
`small-shell` 실행 파일로 제공할 예정이다.

## 개발 규약

- `-std=c99 -Wall -Wextra -Wpedantic` 경고 없이 빌드한다.
- 한 변경은 하나의 명확한 책임만 다룬다.
- 새 추상화는 실제 사용 경로와 함께 검증한다.
- 파일 디스크립터, 동적 메모리와 자식 프로세스의 소유자를 명시한다.
- 실패 경로도 정상 경로와 같은 수준으로 정리한다.
- 동작을 추가한 뒤 독립적으로 재현 가능한 검증을 남긴다.

## 예정 범위

- 인용 문자열과 셸 연산자의 tokenization
- pipeline과 조건 연결자를 포함한 parsing
- 환경 변수와 종료 상태 expansion
- 기본 builtin과 외부 명령 실행
- 파일 redirection과 heredoc

background 실행, job control, subshell, glob과 완전한 POSIX shell 호환은
이 프로젝트의 범위에 포함하지 않는다.

## 검증 원칙

각 단계는 그 시점에 존재하는 코드만으로 빌드할 수 있어야 한다. 기능이 연결된
뒤에는 정상 동작뿐 아니라 입력·할당·시스템 호출 실패와 자원 수명도 검증한다.
