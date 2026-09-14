#ifndef EXEC_INTERNAL_H
# define EXEC_INTERNAL_H

# include "shell.h"

// [INTV:ARCH] 이 헤더는 shell.h(외부에 공개된 자료구조)와 달리 exec.c/redirection.c/
// heredoc.c 세 파일끼리만 공유하는 실행 계층 내부 상태다 — heredoc 본문을 리다이렉션 적용
// 시점까지 들고 있어야 하는데, 그 상태를 t_command/t_redir(파서 계층) 쪽에 얹지 않고 여기
// 별도로 둔 이유는 heredoc.c 파일 자체의 주석 참고.
struct heredoc_entry {
    const t_redir           *redir;
    char                    *body;
    struct heredoc_entry    *next;
};

struct exec_context {
    t_shell                 *shell;
    struct heredoc_entry    *heredocs;
};

int         exec_prepare_heredocs(struct exec_context *ctx,
                t_pipeline *pipelines);
void        exec_heredoc_entries_free(struct heredoc_entry *entry);
const char  *exec_find_heredoc_body(const struct exec_context *ctx,
                const t_redir *redir);
int         exec_apply_redirections(const t_command *command,
                const struct exec_context *ctx);
int         exec_run_parent_command(t_shell *shell, const t_command *command,
                const struct exec_context *ctx);

#endif
