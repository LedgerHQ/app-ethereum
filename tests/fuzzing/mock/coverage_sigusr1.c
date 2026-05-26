// SPDX-FileCopyrightText: © 2026 LEDGER SAS
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include <signal.h>
#include <stddef.h>

extern int __llvm_profile_write_file(void);
extern int __llvm_profile_reset_counters(void);

static void on_sigusr1(int p) {
    int tmp = __llvm_profile_write_file();
    __llvm_profile_reset_counters();  // optional
}

__attribute__((constructor)) static void init(void) {
    struct sigaction sa = {0};
    sa.sa_handler = on_sigusr1;
    sigaction(SIGUSR1, &sa, NULL);
}
