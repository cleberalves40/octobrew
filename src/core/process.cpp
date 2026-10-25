#include "process.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

namespace octo {

namespace {

void closeFd(int &fd)
{
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

bool makePipe(int fds[2])
{
    if (::pipe(fds) != 0)
        return false;
    ::fcntl(fds[0], F_SETFD, FD_CLOEXEC);
    ::fcntl(fds[1], F_SETFD, FD_CLOEXEC);
    return true;
}

} // namespace

std::string findExecutable(const std::string &name)
{
    if (name.find('/') != std::string::npos)
        return ::access(name.c_str(), X_OK) == 0 ? name : std::string();
    const char *path = std::getenv("PATH");
    std::string dirs = path ? path : "/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin:/usr/local/sbin";
    size_t start = 0;
    while (start <= dirs.size()) {
        size_t end = dirs.find(':', start);
        if (end == std::string::npos)
            end = dirs.size();
        const std::string dir = dirs.substr(start, end - start);
        if (!dir.empty()) {
            const std::string candidate = dir + "/" + name;
            struct stat st {};
            if (::stat(candidate.c_str(), &st) == 0 && S_ISREG(st.st_mode)
                && ::access(candidate.c_str(), X_OK) == 0)
                return candidate;
        }
        start = end + 1;
    }
    return std::string();
}

ProcessResult runProcess(const std::vector<std::string> &argv, int timeoutMs, const Environment &env,
                         const OutputFn &onOutput)
{
    ProcessResult r;
    if (argv.empty()) {
        r.startError = "empty command";
        return r;
    }

    // argv e ambiente do filho são montados antes do fork: com threads, o
    // filho não pode alocar memória antes do exec.
    std::vector<std::string> envStrings;
    std::vector<char *> envp;
    if (!env.empty()) {
        for (char **e = environ; e && *e; ++e) {
            const std::string var(*e);
            bool overridden = false;
            for (const auto &kv : env)
                if (var.compare(0, kv.first.size() + 1, kv.first + "=") == 0)
                    overridden = true;
            if (!overridden)
                envStrings.push_back(var);
        }
        for (const auto &kv : env)
            envStrings.push_back(kv.first + "=" + kv.second);
        for (auto &s : envStrings)
            envp.push_back(&s[0]);
        envp.push_back(nullptr);
    }

    std::vector<char *> args;
    args.reserve(argv.size() + 1);
    for (const auto &a : argv)
        args.push_back(const_cast<char *>(a.c_str()));
    args.push_back(nullptr);

    int out[2] = {-1, -1}, err[2] = {-1, -1}, exec[2] = {-1, -1};
    if (!makePipe(out) || !makePipe(err) || !makePipe(exec)) {
        r.startError = std::strerror(errno);
        closeFd(out[0]); closeFd(out[1]); closeFd(err[0]); closeFd(err[1]);
        closeFd(exec[0]); closeFd(exec[1]);
        return r;
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        r.startError = std::strerror(errno);
        closeFd(out[0]); closeFd(out[1]); closeFd(err[0]); closeFd(err[1]);
        closeFd(exec[0]); closeFd(exec[1]);
        return r;
    }

    if (pid == 0) {
        // As consultas não leem nada: sem isso, o pkg disputaria o teclado
        // com a interface.
        const int null = ::open("/dev/null", O_RDONLY);
        if (null >= 0)
            ::dup2(null, STDIN_FILENO);
        ::dup2(out[1], STDOUT_FILENO);
        ::dup2(err[1], STDERR_FILENO);
        if (!env.empty())
            environ = envp.data();   // só uma atribuição: seguro após o fork
        ::execvp(args[0], args.data());
        // Só chega aqui se o exec falhou: avisa o pai pelo pipe de controle,
        // que o exec teria fechado (FD_CLOEXEC).
        const int e = errno;
        (void)::write(exec[1], &e, sizeof e);
        ::_exit(127);
    }

    closeFd(out[1]);
    closeFd(err[1]);
    closeFd(exec[1]);

    int execErrno = 0;
    if (::read(exec[0], &execErrno, sizeof execErrno) == static_cast<ssize_t>(sizeof execErrno)) {
        closeFd(exec[0]);
        closeFd(out[0]);
        closeFd(err[0]);
        ::waitpid(pid, nullptr, 0);
        r.startError = std::strerror(execErrno);
        return r;
    }
    closeFd(exec[0]);
    r.started = true;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    char buf[4096];
    while (out[0] >= 0 || err[0] >= 0) {
        struct pollfd fds[2];
        int n = 0;
        if (out[0] >= 0)
            fds[n++] = {out[0], POLLIN, 0};
        if (err[0] >= 0)
            fds[n++] = {err[0], POLLIN, 0};

        int wait = -1;
        if (timeoutMs >= 0) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            if (left <= 0) {
                r.timedOut = true;
                break;
            }
            wait = static_cast<int>(left);
        }
        const int ready = ::poll(fds, n, wait);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (ready == 0)
            continue;   // o laço reavalia o prazo
        for (int i = 0; i < n; ++i) {
            if (!(fds[i].revents & (POLLIN | POLLHUP | POLLERR)))
                continue;
            const ssize_t got = ::read(fds[i].fd, buf, sizeof buf);
            int &fd = fds[i].fd == out[0] ? out[0] : err[0];
            std::string &dest = fds[i].fd == out[0] ? r.out : r.err;
            if (got > 0) {
                dest.append(buf, static_cast<size_t>(got));
                if (onOutput)
                    onOutput(std::string(buf, static_cast<size_t>(got)));
            }
            else if (got == 0 || errno != EINTR)
                closeFd(fd);
        }
    }

    if (r.timedOut)
        ::kill(pid, SIGKILL);
    closeFd(out[0]);
    closeFd(err[0]);

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    if (WIFEXITED(status))
        r.status = WEXITSTATUS(status);
    else if (WIFSIGNALED(status) && !r.timedOut)
        r.crashed = true;
    return r;
}

} // namespace octo
