#!/usr/bin/env python3
# Compile the app with MEMORY_PROFILING=1 and feed the speculos output to this script :
# pytest --device nanosp -s -k my_special_test 2>&1 | ./valground.py

import sys
from argparse import ArgumentParser, Namespace, ArgumentDefaultsHelpFormatter, RawDescriptionHelpFormatter
from typing import Optional


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
    persists: dict[Allocation]
    allocd_overtime: int
    allocd_max: int
    allocd_current: int
    alloc_count: int
    addr: int
    size: int
    test_name: str
    free_errors: list[str]

    def __init__(self, addr: int, size: int, test_name: str) -> None:
        self.allocs = {}
        self.persists = {}
        self.allocd_overtime = 0
        self.allocd_max = 0
        self.allocd_current = 0
        self.alloc_count = 0
        self.addr = addr
        self.size = size
        self.test_name = test_name
        self.free_errors = []

    def alloc(self, ptr: int, size: int, code_loc: str, persist: bool) -> None:
        if persist:
            self.persists[ptr] = Allocation(size, code_loc)
        else:
            self.allocs[ptr] = Allocation(size, code_loc)
        self.allocd_current += size
        self.allocd_max = max(self.allocd_max, self.allocd_current)
        self.allocd_overtime += size
        self.alloc_count += 1

    def free(self, ptr: int, code_loc: str) -> None:
        if ptr not in self.allocs and ptr not in self.persists:
            self.free_errors.append(code_loc)
            return
        if ptr in self.allocs:
            self.allocd_current -= self.allocs[ptr].size
            del self.allocs[ptr]
        else:
            self.allocd_current -= self.persists[ptr].size
            del self.persists[ptr]

    def summary(self, quiet: bool = False) -> bool:
        print("")
        if self.test_name:
            print(f"{COLORS['fg_blue']}{COLORS['bg_white']}{self.test_name}{COLORS['all_reset']}")

        if len(self.free_errors) == 0:
            if not quiet:
                print(f"{COLORS['fg_green']}No memory free errors detected, congrats!{COLORS['all_reset']}")
        else:
            print(f"{COLORS['fg_red']}Free without malloc, or double free detected:")
            for code_loc in self.free_errors:
                print(f"- {code_loc}")
            print(f"{COLORS['all_reset']}", end='')

        if len(self.allocs) == 0:
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
            if len(self.persists) > 0:
                print(f"{COLORS['fg_yellow']}Persistent memory allocations:")
                for addr, info in list(self.persists.items()):
                    print(f"- {info.size} bytes from {info.code_location}")
                    del self.persists[addr]
                print(f"{COLORS['all_reset']}", end='')

        return len(self.allocs) + len(self.free_errors) == 0


# ===============================================================================
#          Init colors
# ===============================================================================
def init_colors(enable_colors: bool) -> dict[str, str]:
    """Initialize color variables based on colorama availability

    ARGS:
        enable_colors (bool): Flag to enable or disable colors
    """
    colors = {
        'fg_green': '',
        'fg_red': '',
        'all_reset': '',
        'fg_yellow': '',
        'fg_blue': '',
        'bg_white': ''
    }

    if enable_colors:
        try:
            import colorama
            colorama.init()  # Initialize colorama
            colors.update({
                'fg_green': colorama.Fore.GREEN,
                'fg_red': colorama.Fore.RED,
                'all_reset': colorama.Fore.RESET + colorama.Back.RESET,
                'fg_yellow': colorama.Fore.YELLOW,
                'fg_blue': colorama.Fore.BLUE,
                'bg_white': colorama.Back.WHITE
            })
        except ImportError:
            print("To use colors, please install colorama: pip install colorama")

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
    parser.add_argument("--colors", "-c", action='store_true', help="Enable colored output.")
    return parser.parse_args()


# ===============================================================================
#          Main entry
# ===============================================================================
def main() -> None:
    mem: Optional[Memory] = None

    # Arguments parsing
    # -----------------
    args = init_parser()

    # Initialize colors once at module level
    global COLORS
    COLORS = init_colors(args.colors)

   # Processing
    # ----------
    ret_code = 0
    testnb = 0
    test_name = ""
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
                    # Print summary of previous test if exists
                    if mem is not None:
                        if mem.summary(args.quiet) is False:
                            ret_code = 1
                    mem = Memory(int(words[1], base=0), int(words[2], base=0), test_name)
                elif words[0] in ("alloc", "persist"):
                    mem.alloc(int(words[2], base=0), int(words[1], base=0), words[3], words[0] == "persist")
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


global_summary = GlobalSummary()

if __name__ == "__main__":
    main()
