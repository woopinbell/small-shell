# Small shell

![Language](https://img.shields.io/badge/language-C99-blue?logo=c&logoColor=white)
![Platform](https://img.shields.io/badge/platform-POSIX-lightgrey)

`small-shell`은 42 `minishell` 과제를 변형한 C 프로젝트입니다. 명령줄을 읽고 tokenization, parsing, expansion, redirection, heredoc, pipeline과 builtin 실행을 수행하는 작은 POSIX 스타일 셸입니다.

## 지원 범위

- 따옴표와 셸 연산자를 포함한 입력 tokenization
- 환경 변수와 종료 상태 expansion
- pipeline과 redirection
- heredoc
- builtin: `cd`, `echo`, `env`, `exit`, `export`, `pwd`, `unset`
- 외부 명령 실행과 자식 프로세스 상태 관리
- 오류와 시스템 호출 실패 경로의 자원 정리

완전한 POSIX 셸 호환, job control, subshell과 glob은 범위에 포함하지 않습니다.

## 빌드

저장소 루트에서 실행합니다.

```sh
make
```

일반 실행 파일은 `build/bin/`, 테스트 실행 파일은 `build/test/`, sanitizer 실행 파일은 `build/san/`, 오브젝트와 dependency 파일은 `build/obj/`에 생성됩니다.

readline을 활성화해 빌드하려면 다음과 같이 실행합니다.

```sh
make readline
```

## 사용 예시

```sh
./build/bin/small-shell
```

셸에서 다음과 같은 명령을 실행할 수 있습니다.

```sh
echo hello | wc -c
export NAME=world
echo "hello $NAME"
cat <<EOF
heredoc
EOF
```

## 테스트

기본 테스트는 정상 실행, parser API, fault injection, allocation failure, lifecycle과 긴 입력을 확인합니다.

```sh
make test
```

AddressSanitizer와 UndefinedBehaviorSanitizer 테스트는 각각 다음 명령으로 실행합니다.

```sh
make test-asan
make test-ubsan
```

Docker 환경에서 sanitizer를 실행하려면 다음 명령을 사용합니다.

```sh
make test-sanitizers-container
```

## 정리

```sh
make clean  # build/ 및 테스트 캐시 삭제
make fclean # clean과 동일
make re     # fclean 후 전체 재빌드
```

빌드 산출물과 테스트 캐시는 저장소에 포함하지 않습니다.
