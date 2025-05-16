#!/usr/bin/env python3
# Compile the app with MEMORY_PROFILING=1 and feed the speculos output to this script :
# pytest --device nanosp -s -k my_special_test 2>&1 | ./valground.py

import sys


class Allocation:
    size: int
    code_location: str

    def __init__(self, size: int, code_location: str):
        self.size = size
        self.code_location = code_location


class Memory:
    allocs: dict[Allocation]
    allocd_overtime: int
    allocd_max: int
    allocd_current: int
    alloc_count: int

    def __init__(self):
        self.allocs = {}
        self.allocd_overtime = 0
        self.allocd_max = 0
        self.allocd_current = 0
        self.alloc_count = 0

    def alloc(self, ptr: int, size: int, code_loc: str):
        self.allocs[ptr] = Allocation(size, code_loc)
        self.allocd_current += size
        if self.allocd_current > self.allocd_max:
            self.allocd_max = self.allocd_current
        self.allocd_overtime += size
        self.alloc_count += 1

    def free(self, ptr: int, code_loc: str):
        assert ptr in self.allocs
        self.allocd_current -= self.allocs[ptr].size
        del self.allocs[ptr]


mem = Memory()

for line in sys.stdin:
    line = line.rstrip()
    scheme = "seproxyhal: printf: "
    if line.startswith(scheme):
        line = line[len(scheme):]
        scheme = "==MP "
        if line.startswith(scheme):
            line = line[len(scheme):]
            words = line.split(";")
            assert len(words) > 2

            if words[0] == "alloc":
                mem.alloc(int(words[2], base=0), int(words[1], base=0), words[3])
            elif words[0] == "free":
                mem.free(int(words[1], base=0), words[2])
            else:
                assert False

if len(mem.allocs) == 0:
    print("No memory leak detected, congrats!")
else:
    print("Memory leaks :")
    for addr, info in mem.allocs.items():
        print("- [0x%.08x] %u bytes from %s" % (addr, info.size, info.code_location))

print("\n=== Summary ===")
print("Total overtime = %u bytes" % (mem.allocd_overtime))
print("Max overtime = %u bytes" % (mem.allocd_max))
print("Allocations = %u" % (mem.alloc_count))
