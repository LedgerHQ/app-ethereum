#!/usr/bin/env python3
# Compile the app with MEMORY_PROFILING=1 and feed the speculos output to this script :
# pytest --device nanosp -s -k my_special_test 2>&1 | ./valground.py

import sys
from argparse import ArgumentParser, Namespace, ArgumentDefaultsHelpFormatter, RawDescriptionHelpFormatter
from typing import Optional, Dict
# Check colorama availability at module level
HAS_COLORAMA = False
try:
    from colorama import init, Fore, Back
    # init()  # Initialize colorama
    HAS_COLORAMA = True
except ImportError:
     print("To use colors, please install colorama: pip install colorama")


# ===============================================================================
#          Summary class
# ===============================================================================
class GlobalSummary:
    max_size: int
    code_location: str

    def __init__(self):
        self.max_size = 0

    def update(self, size: int):
        self.max_size = max(size, self.max_size)

    def summary(self):
        print(f"\n{COLORS['fg_red']}{COLORS['bg_white']}=== Global ==={COLORS['all_reset']}")
        print(f"{COLORS['fg_yellow']}Higher Max overtime = {self.max_size} bytes{COLORS['all_reset']}\n")


# ===============================================================================
#          Allocation class
# ===============================================================================
class Allocation:
    size: int
    code_location: str

    def __init__(self, size: int, code_location: str):
        self.size = size
        self.code_location = code_location


# ===============================================================================
#          Memory class
# ===============================================================================
class Memory:
    allocs: dict[Allocation]
    allocd_overtime: int
    allocd_max: int
    allocd_current: int
    alloc_count: int
    addr: int
    size: int

    def __init__(self, addr: int, size: int, test_name: str) -> None:
        self.allocs = {}
        self.allocd_overtime = 0
        self.allocd_max = 0
        self.allocd_current = 0
        self.alloc_count = 0
        self.addr = addr
        self.size = size
        self.test_name = test_name

    def alloc(self, ptr: int, size: int, code_loc: str) -> None:
        self.allocs[ptr] = Allocation(size, code_loc)
        self.allocd_current += size
        self.allocd_max = max(self.allocd_max, self.allocd_current)
        self.allocd_overtime += size
        self.alloc_count += 1

    def free(self, ptr: int, code_loc: str) -> None:
        assert ptr in self.allocs
        self.allocd_current -= self.allocs[ptr].size
        del self.allocs[ptr]

    def summary(self, quiet: bool = False) -> bool:
        print(f"\n{COLORS['fg_blue']}{COLORS['bg_white']}{self.test_name}{COLORS['all_reset']}")
        ret: bool = False
        if len(self.allocs) == 0:
            ret = True
            if not quiet:
                print(f"{COLORS['fg_green']}No memory leak detected, congrats!{COLORS['all_reset']}")
        else:
            print(f"{COLORS['fg_red']}Memory leaks:")
            for addr, info in self.allocs.items():
                print("- [0x%.08x] %u bytes from %s" % (addr,
                                                        info.size,
                                                        info.code_location))
            print(f"{COLORS['all_reset']}", end='')

        global_summary.update(self.allocd_max)
        if not quiet:
            print("=== Summary ===")
            print("Total overtime = %u bytes" % (self.allocd_overtime))
            used_percentage = self.allocd_max / self.size * 100
            info_str = f"Max overtime = {self.allocd_max} bytes ({used_percentage:.02f}% full)"
            if used_percentage > 90:
                print(f"{COLORS['fg_yellow']}{info_str}{COLORS['all_reset']}")
            else:
                print(info_str)
            print("Allocations = %u" % (self.alloc_count))

        return ret


# ===============================================================================
#          Init colors
# ===============================================================================
def init_colors() -> Dict[str, str]:
    """Initialize color variables based on colorama availability"""
    colors = {
        'fg_green': '',
        'fg_red': '',
        'all_reset': '',
        'fg_yellow': '',
        'fg_blue': '',
        'bg_white': ''
    }

    if HAS_COLORAMA:
        init()  # Initialize colorama
        colors.update({
            'fg_green': Fore.GREEN,
            'fg_red': Fore.RED,
            'all_reset': Fore.RESET + Back.RESET,
            'fg_yellow': Fore.YELLOW,
            'fg_blue': Fore.BLUE,
            'bg_white': Back.WHITE
        })

    return colors


# ===============================================================================
#          Parameters
# ===============================================================================
class CustomFormatter(ArgumentDefaultsHelpFormatter, RawDescriptionHelpFormatter):
    """Custom formatter that combines ArgumentDefaultsHelpFormatter and RawDescriptionHelpFormatter"""
    pass

def init_parser() -> Namespace:
    """Initialize the argument parser for command line arguments"""
    epilog = "Compile the app with MEMORY_PROFILING=1 and feed the speculos output to this script.\n"
    epilog += "Example usage: pytest --device nanosp -s -k my_special_test 2>&1 | ./valground.py"
    parser = ArgumentParser(description="Analyze memory allocations to determine leaks.",
                            formatter_class=CustomFormatter,
                            epilog=epilog)
    parser.add_argument("--quiet", "-q", action='store_true', help="Quiet logs to minimum.")
    return parser.parse_args()


# ===============================================================================
#          Main entry
# ===============================================================================
def main() -> None:
    mem: Optional[Memory] = None

    # Arguments parsing
    # -----------------
    args = init_parser()

    # Processing
    # ----------
    ret_code = 0
    testnb = 0
    for line in sys.stdin:
        line = line.rstrip()

        # Only print lines that look like pytest test names
        if line.startswith("collecting"):
            line = line.replace("collecting ... ", "")
        if line.startswith("collected"):
            print(line)
            nb_tests = int(line.split(" ")[-2])
            continue
        if "test_" in line and "::" in line and "[" in line:
            # Print summary of previous test if exists
            if mem is not None:
                if mem.summary(args.quiet) is False:
                    ret_code = 1
            testnb += 1
            test_name = f"[{testnb}/{nb_tests}] - {line.split(' ')[0]}"
            continue

        scheme = "seproxyhal: printf: "
        if line.startswith(scheme):
            line = line[len(scheme):]
            scheme = "==MP "
            if line.startswith(scheme):
                line = line[len(scheme):]
                words = line.split(";")
                assert len(words) > 2

                if words[0] == "init":
                    mem = Memory(int(words[1], base=0), int(words[2], base=0), test_name)
                elif words[0] == "alloc":
                    mem.alloc(int(words[2], base=0), int(words[1], base=0), words[3])
                elif words[0] == "free":
                    mem.free(int(words[1], base=0), words[2])
                else:
                    assert False

    if mem is None:
        print(f"{COLORS['fg_red']}No memory profiling was found.{COLORS['all_reset']}")
    else:
        if mem.summary(args.quiet) is False:
            ret_code = 1
        global_summary.summary()
    sys.exit(ret_code)


# Initialize colors once at module level
COLORS = init_colors()
global_summary = GlobalSummary()

if __name__ == "__main__":
    main()
