#pragma once

#include "exec/io/stream/Stream.h"

#include <supp/verify.h>

#include <cstddef>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace exec {

struct IOStream : public Stream {
    IOStream() {
        setNonBlock();
        enableRawTerminalMode();
    }

    ~IOStream() {
        resetIO();
        resetTerminal();
    }

    int read() override {
        char c = 0;
        auto bytes_read = ::read(STDIN_FILENO, &c, 1);
        return bytes_read > 0 ? (int)c : -1;
    }

    size_t write(const char* s, size_t cnt) override {
        auto bytes_written = ::write(STDOUT_FILENO, s, cnt);
        return bytes_written > 0 ? bytes_written : 0;
    }

    int available() override {
        DASSERT(false, "not implemented");
        return 0;
    }

    int peek() override {
        DASSERT(false, "not implemented");
        return -1;
    }

    int availableForWrite() override {
        DASSERT(false, "not implemented");
        return 0;
    }

 private:
    void setNonBlock() {
        stdin_flags_orig_ = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, stdin_flags_orig_ | O_NONBLOCK);

        stdout_flags_orig_ = fcntl(STDOUT_FILENO, F_GETFL, 0);
        fcntl(STDOUT_FILENO, F_SETFL, stdout_flags_orig_ | O_NONBLOCK);
    }

    void resetIO() {
        fcntl(STDIN_FILENO, F_SETFL, stdin_flags_orig_);
        fcntl(STDIN_FILENO, F_SETFL, stdout_flags_orig_);
    }

    void enableRawTerminalMode() {
        termios raw{};
        tcgetattr(STDIN_FILENO, &t_orig_);
        raw = t_orig_;
        raw.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }

    void resetTerminal() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &t_orig_); }

    int stdin_flags_orig_{};
    int stdout_flags_orig_{};
    termios t_orig_{};
};

}  // namespace exec

inline exec::IOStream Serial;
