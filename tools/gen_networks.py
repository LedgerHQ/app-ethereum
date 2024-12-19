#!/usr/bin/env python3

import os
import sys
import re
import argparse
from pathlib import Path


class Network:
    chain_id: int
    name: str
    ticker: str

    def __init__(self, chain_id: int, name: str, ticker: str):
        self.chain_id = chain_id
        self.name = name
        self.ticker = ticker


def get_network_glyph_name(net: Network) -> str:
    return "chain_%u_64px" % (net.chain_id)


def get_header() -> str:
    return """\
/*
 * Generated by %s
 */

""" % (sys.argv[0])


def gen_icons_array_inc(networks: list[Network], path: str) -> bool:
    with open(path + ".h", "w") as out:
        print(get_header() + """\
#ifndef NETWORK_ICONS_GENERATED_H_
#define NETWORK_ICONS_GENERATED_H_

#include <stdint.h>
#include "nbgl_types.h"

typedef struct {
    uint64_t chain_id;
    const nbgl_icon_details_t *icon;
} network_icon_t;

extern const network_icon_t g_network_icons[%u];

#endif // NETWORK_ICONS_GENERATED_H_ \
""" % (len(networks)), file=out)
    return True


def gen_icons_array_src(networks: list[Network], path: str) -> bool:
    with open(path + ".c", "w") as out:
        print(get_header() + """\
#include "glyphs.h"
#include "%s.h"

const network_icon_t g_network_icons[%u] = {\
""" % (os.path.basename(path), len(networks)), file=out)

        for net in networks:
            glyph_name = get_network_glyph_name(net)
            glyph_file = "glyphs/%s.gif" % (glyph_name)
            if os.path.isfile(glyph_file):
                if os.path.islink(glyph_file):
                    glyph_name = Path(os.path.realpath(glyph_file)).stem
                print(" "*4, end="", file=out)
                print("{.chain_id = %u, .icon = &C_%s}, // %s" % (net.chain_id,
                                                                  glyph_name,
                                                                  net.name),
                      file=out)

        print("};", file=out)
    return True


def gen_icons_array(networks: list[Network], path: str) -> bool:
    path += "/net_icons.gen"
    if not gen_icons_array_inc(networks, path) or \
       not gen_icons_array_src(networks, path):
        return False
    return True


def network_icon_exists(net: Network) -> bool:
    return os.path.isfile("glyphs/%s.gif" % (get_network_glyph_name(net)))


def main(output_dir: str) -> bool:
    networks: list[Network] = list()

    # get chain IDs and network names
    expr = r"{\.chain_id = ([0-9]*), \.name = \"(.*)\", \.ticker = \"(.*)\"},"
    with open("src/network.c") as f:
        for line in f.readlines():
            line = line.strip()
            if line.startswith("{") and line.endswith("},"):
                m = re.search(expr,
                              line)
                assert(m.lastindex == 3)
                if m.group(1) != "1":
                    continue
                networks.append(Network(int(m.group(1)),
                                        m.group(2),
                                        m.group(3)))

    networks.sort(key=lambda x: x.chain_id)

    if not gen_icons_array(list(filter(network_icon_exists, networks)),
                           output_dir):
        return False
    return True


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("OUTPUT_DIR")
    args = parser.parse_args()
    assert os.path.isdir(args.OUTPUT_DIR)
    quit(0 if main(args.OUTPUT_DIR) else 1)
