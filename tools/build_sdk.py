#!/usr/bin/env python3

'''
This script extract a few specific definitions from app-ethereum that are
required to exchange information with ethereum external plugins.
It should always be launched from app-ethereum:

python3 ethereum-plugin-sdk/build_sdk.py

'''

import os


def extract_from_headers(sources, nodes_to_extract):
    cat_sources = []
    for source in sources:
        with open(source, 'r') as f:
            cat_sources += f.readlines()

    sdk_body = []
    for key, values in nodes_to_extract.items():
        for value in values:
            node = []
            unclosed_curvy_brackets = False
            unclosed_parantheses = False
            for line in cat_sources:
                if key in line and value in line:
                    node += [line]
                    unclosed_curvy_brackets = line.count('{') - line.count('}')
                    if unclosed_curvy_brackets == False:
                        break
                elif (key == "fn" and value in line) or unclosed_parantheses:
                    node += [line]
                    unclosed_parantheses = line.find(")") == -1
                    if unclosed_parantheses == False:
                        break
                elif unclosed_curvy_brackets:
                    node += [line]
                    unclosed_curvy_brackets += line.count(
                        '{') - line.count('}')
                    if unclosed_curvy_brackets:
                        continue
                    else:
                        break

            sdk_body += [''.join(node)]

    return '\n'.join(sdk_body)


def extract_from_c_files(sources, nodes_to_extract):
    cat_sources = []
    for source in sources:
        with open(source, 'r') as f:
            cat_sources += f.readlines()

    sdk_body = []
    for node_name in nodes_to_extract:
        node = []
        copying = False
        wait_curvy_bracket = True
        for line in cat_sources:
            if node_name in line:
                copying = True
                node += [line]
                unclosed_curvy_brackets = line.count('{') - line.count('}')
            elif copying:
                node += [line]
                unclosed_curvy_brackets += line.count('{') - line.count('}')
                if wait_curvy_bracket:
                    wait_curvy_bracket = line.count('}') == 0
                if unclosed_curvy_brackets != 0 or wait_curvy_bracket:
                    continue
                else:
                    break

        sdk_body += [''.join(node)]

    return '\n'.join(sdk_body)


def merge_headers(sources, nodes_to_extract):
    includes = [
        '#include "os.h"',
        '#include "cx.h"',
        '#include <stdbool.h>',
        '#include <string.h>'
    ]

    body = extract_from_headers(sources, nodes_to_extract)

    eth_internals_h = '\n\n'.join([
        "/* This file is auto-generated, don't edit it */",
        "#pragma once",
        '\n'.join(includes),
        body
    ])

    with open("ethereum-plugin-sdk/include/eth_internals.h", 'w') as f:
        f.write(eth_internals_h)


def copy_header(header_to_copy, merged_headers):

    merged_headers = [os.path.basename(path) for path in merged_headers]

    with open(header_to_copy, 'r') as f:
        source = f.readlines()

    eth_plugin_interface_h = [
        "/* This file is auto-generated, don't edit it */\n"]
    for line in source:
        eth_plugin_interface_h += [line]
        for header in merged_headers:
            if header in line:
                del eth_plugin_interface_h[-1]
                break

    # add '#include "eth_internals.h"'
    include_index = eth_plugin_interface_h.index('#include "cx.h"\n')
    eth_plugin_interface_h.insert(
        include_index+1, '#include "eth_internals.h"\n')

    # dump to file
    with open("ethereum-plugin-sdk/include/eth_plugin_interface.h", 'w') as f:
        f.writelines(eth_plugin_interface_h)


def merge_c_files(sources, nodes_to_extract):
    includes = [
        '#include "eth_internals.h"'
    ]

    body = extract_from_c_files(sources, nodes_to_extract)

    eth_internals_h = '\n\n'.join([
        "/* This file is auto-generated, don't edit it */",
        '\n'.join(includes),
        body
    ])

    with open("ethereum-plugin-sdk/include/eth_internals.c", 'w') as f:
        f.write(eth_internals_h)


if __name__ == "__main__":

    # some nodes will be extracted from these headers and merged into a new one, copied to sdk
    headers_to_merge = [
        "src/tokens.h",
        "src/chainConfig.h",
        "src/utils.h",
        "src_common/ethUstream.h",
        "src_common/ethUtils.h",
        "src/shared_context.h",
        "src/eth_plugin_internal.h",
        "src/nft.h",
    ]
    nodes_to_extract = {
        "#define": ["MAX_TICKER_LEN", "ADDRESS_LENGTH", "INT256_LENGTH", "WEI_TO_ETHER", "SELECTOR_SIZE", "PARAMETER_LENGTH", "RUN_APPLICATION", "COLLECTION_NAME_MAX_LEN"],
        "typedef enum": [],
        "typedef struct": ["tokenDefinition_t", "txInt256_t", "txContent_t", "nftInfo_t"],
        "typedef union": ["extraInfo_t"],
        "__attribute__((no_instrument_function)) inline": ["int allzeroes"],
        "const": ["HEXDIGITS"],
        "fn": ["void getEthAddressStringFromBinary", "void getEthAddressFromKey", "void getEthDisplayableAddress", "bool adjustDecimals", "bool uint256_to_decimal", "void amountToString", "void u64_to_string", "void copy_address", "void copy_parameter"]
    }
    merge_headers(headers_to_merge, nodes_to_extract)

    # this header will be stripped from all #include related to previously merged headers, then copied to sdk
    copy_header("src/eth_plugin_interface.h", headers_to_merge)

    # extract and merge function bodies
    c_files_to_merge = [
        "src/utils.c",
        "src_common/ethUtils.c",
        "src/eth_plugin_internal.c"
    ]
    merge_c_files(c_files_to_merge, nodes_to_extract["fn"])
