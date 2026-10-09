// ctron_proc —— 进程控制垫片(lib/proc;CW-F3b 2026-10-09)
// 定位:零 shell 进程面——fork+execv 直跑(argv 直构,无字符串拼接命令行,零注入面);
//       前台(捕获 stdout,wait 内联)与后台(daemon 形:不 wait,stdout→/dev/null,
//       stderr→err_path 落盘取证)两族 + kill/wait 收尸。
// 来源:port 自 loom c_src/loom_exec.c 全件 + loom_net.c 进程三件(spawn/kill/wait)
// (loom C 垫片内化战役批3b-i)。
// ABI 纪律:ctron_view_w8u 与发射器模板逐字段一致;Str = GC NUL 结尾串;
// argv blob = NUL 分隔参数块(首段 = 程序路径,逻辑长度 n 显式传)。
// rc 口径:run 族 = 进程退出码(WIFEXITED;fork/chdir/管道败 = -1;exec 失败 = 127);
// spawn_bg = pid(-1 败);kill = kill() rc;wait = 退出码(信号杀 = -1)。
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

typedef struct { uint8_t* d; int64_t n; } ctron_view_w8u;

// 管道收 stdout(wait 前读尽,防管道满死锁);读到的字节数随 *captured 回
static int run_capture_child_wait(int pipefd[2], pid_t pid, ctron_view_w8u out) {
    close(pipefd[1]);
    int64_t total = 0;
    while (total < out.n) {
        ssize_t r = read(pipefd[0], out.d + total, (size_t)(out.n - total));
        if (r <= 0) { break; }
        total += r;
    }
    close(pipefd[0]);
    int st = 0;
    waitpid(pid, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

// 在 cwd 下执行 "<bin> run <entry>"(argv 直构;cwd 空串 = 继承——chdir("") 恒 ENOENT 显式跳过)
int32_t ctron_proc_run(const char* cwd, const char* bin, const char* arg, ctron_view_w8u out) {
    int pipefd[2];
    if (pipe(pipefd) != 0) { return -1; }
    pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return -1; }
    if (pid == 0) {
        if (cwd[0] != 0 && chdir(cwd) != 0) { _exit(126); }
        dup2(pipefd[1], 1);
        close(pipefd[0]);
        close(pipefd[1]);
        char* argv[4];
        argv[0] = (char*)bin; argv[1] = (char*)"run"; argv[2] = (char*)arg; argv[3] = NULL;
        execv(bin, argv);
        _exit(127);
    }
    return run_capture_child_wait(pipefd, pid, out);
}

// 通用 argv 形:blob = NUL 分隔参数块(首段 = 程序路径,n = 逻辑字节数);argv 上限 12
int32_t ctron_proc_run_argv(const char* cwd, ctron_view_w8u blob, int64_t n, ctron_view_w8u out) {
    char* argv[13];
    int argc = 0;
    int64_t i = 0;
    while (i < n && argc < 12) {
        argv[argc++] = (char*)(blob.d + i);
        while (i < n && blob.d[i] != 0) { i++; }
        i++;
    }
    if (argc == 0) { return -1; }
    argv[argc] = NULL;
    int pipefd[2];
    if (pipe(pipefd) != 0) { return -1; }
    pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return -1; }
    if (pid == 0) {
        if (cwd[0] != 0 && chdir(cwd) != 0) { _exit(126); }
        dup2(pipefd[1], 1);
        close(pipefd[0]);
        close(pipefd[1]);
        execv(argv[0], argv);
        _exit(127);
    }
    return run_capture_child_wait(pipefd, pid, out);
}

// 后台进程(daemon 形):fork 不 wait;stdout → /dev/null;stderr → err_path 追加落盘
// (err_path 空串 = /dev/null);argv = "<bin> run <entry>"。返回 pid(-1 = fork/chdir 败)。
// stderr 落文件取证 = loom daemon 传承(运行时错误曾随 /dev/null 吞掉——静默退出定位三小时实测)
int64_t ctron_proc_spawn_bg(const char* cwd, const char* bin, const char* arg, const char* err_path) {
    pid_t pid = fork();
    if (pid < 0) { return -1; }
    if (pid == 0) {
        if (cwd[0] != 0 && chdir(cwd) != 0) { _exit(126); }
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, 1); }
        int errfd = -1;
        if (err_path[0] != 0) { errfd = open(err_path, O_WRONLY | O_CREAT | O_APPEND, 0644); }
        if (errfd < 0) { errfd = devnull; }
        if (errfd >= 0) { dup2(errfd, 2); }
        char* argv[4];
        argv[0] = (char*)bin; argv[1] = (char*)"run"; argv[2] = (char*)arg; argv[3] = NULL;
        execv(bin, argv);
        _exit(127);
    }
    return (int64_t)pid;
}

int32_t ctron_proc_kill(int64_t pid) {
    return kill((pid_t)pid, SIGTERM);
}

int32_t ctron_proc_wait(int64_t pid) {
    int st = 0;
    waitpid((pid_t)pid, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

// git 桥等工具面原语:PATH 查找(execvp)+ stdin 重定向 + 可选捕获。
// blob = NUL 分隔 argv 块(首段 = 程序名,execvp 按 PATH 解析;argv 上限 12);
// stdin_path 空串 = 继承,非空 = 其内容重定向进 stdin(fast-import 流式面);
// capture = 0 → 不建管道,子进程 stdout 继承(rc 型工具面——截断管道会 SIGPIPE 误杀
// 长输出的 push/fetch,loom_git.c 传承语义);capture > 0 → 管道捕获至 out 容量,
// *out_len = 实捕字节数。返回进程退出码(-1 = fork/chdir/管道/exec 链败,127 = exec 失败)。
typedef struct { int64_t v; } ProcBox64;

int32_t ctron_proc_run_argv_p(const char* cwd, ctron_view_w8u blob, int64_t n, const char* stdin_path, int64_t capture, ctron_view_w8u out, ProcBox64* out_len) {
    char* argv[13];
    int argc = 0;
    int64_t i = 0;
    while (i < n && argc < 12) {
        argv[argc++] = (char*)(blob.d + i);
        while (i < n && blob.d[i] != 0) { i++; }
        i++;
    }
    if (argc == 0) { return -1; }
    argv[argc] = NULL;
    int pipefd[2];
    int have_pipe = (capture > 0 && out.d != NULL && out.n > 0);
    if (have_pipe && pipe(pipefd) != 0) { return -1; }
    pid_t pid = fork();
    if (pid < 0) {
        if (have_pipe) { close(pipefd[0]); close(pipefd[1]); }
        return -1;
    }
    if (pid == 0) {
        if (cwd[0] != 0 && chdir(cwd) != 0) { _exit(126); }
        if (stdin_path[0] != 0) {
            int fd = open(stdin_path, O_RDONLY);
            if (fd < 0) { _exit(127); }
            dup2(fd, 0);
            close(fd);
        }
        if (have_pipe) {
            dup2(pipefd[1], 1);
            close(pipefd[0]);
            close(pipefd[1]);
        }
        execvp(argv[0], argv);
        _exit(127);
    }
    if (have_pipe) { close(pipefd[1]); }
    int64_t total = 0;
    if (have_pipe) {
        while (total < out.n) {
            ssize_t r = read(pipefd[0], out.d + total, (size_t)(out.n - total));
            if (r <= 0) { break; }
            total += r;
        }
        close(pipefd[0]);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    if (out_len != NULL) { out_len->v = have_pipe ? total : 0; }
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}
