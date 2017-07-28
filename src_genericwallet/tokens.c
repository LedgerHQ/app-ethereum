/*******************************************************************************
*   Ledger Blue
*   (c) 2016 Ledger
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

#include "tokens.h"

const tokenDefinition_t const TOKENS[NUM_TOKENS] = {

    {{0xAf, 0x30, 0xD2, 0xa7, 0xE9, 0x0d, 0x7D, 0xC3, 0x61, 0xc8,
      0xC4, 0x58, 0x5e, 0x9B, 0xB7, 0xD2, 0xF6, 0xf1, 0x5b, 0xc7},
     "1ST ",
     18},
    {{0x42, 0x28, 0x66, 0xa8, 0xF0, 0xb0, 0x32, 0xc5, 0xcf, 0x1D,
      0xfB, 0xDE, 0xf3, 0x1A, 0x20, 0xF4, 0x50, 0x95, 0x62, 0xb0},
     "ADST ",
     0},
    {{0xD0, 0xD6, 0xD6, 0xC5, 0xFe, 0x4a, 0x67, 0x7D, 0x34, 0x3c,
      0xC4, 0x33, 0x53, 0x6B, 0xB7, 0x17, 0xbA, 0xe1, 0x67, 0xdD},
     "ADT ",
     9},
    {{0x44, 0x70, 0xBB, 0x87, 0xd7, 0x7b, 0x96, 0x3A, 0x01, 0x3D,
      0xB9, 0x39, 0xBE, 0x33, 0x2f, 0x92, 0x7f, 0x2b, 0x99, 0x2e},
     "ADX ",
     4},
    {{0x96, 0x0b, 0x23, 0x6A, 0x07, 0xcf, 0x12, 0x26, 0x63, 0xc4,
      0x30, 0x33, 0x50, 0x60, 0x9A, 0x66, 0xA7, 0xB2, 0x88, 0xC0},
     "ANT ",
     18},
    {{0xAc, 0x70, 0x9F, 0xcB, 0x44, 0xa4, 0x3c, 0x35, 0xF0, 0xDA,
      0x4e, 0x31, 0x63, 0xb1, 0x17, 0xA1, 0x7F, 0x37, 0x70, 0xf5},
     "ARC ",
     18},
    {{0x0D, 0x87, 0x75, 0xF6, 0x48, 0x43, 0x06, 0x79, 0xA7, 0x09,
      0xE9, 0x8d, 0x2b, 0x0C, 0xb6, 0x25, 0x0d, 0x28, 0x87, 0xEF},
     "BAT ",
     18},
    {{0x74, 0xC1, 0xE4, 0xb8, 0xca, 0xE5, 0x92, 0x69, 0xec, 0x1D,
      0x85, 0xD3, 0xD4, 0xF3, 0x24, 0x39, 0x60, 0x48, 0xF4, 0xac},
     "(^) ",
     0},
    {{0x1e, 0x79, 0x7C, 0xe9, 0x86, 0xC3, 0xCF, 0xF4, 0x47, 0x2F,
      0x7D, 0x38, 0xd5, 0xC4, 0xab, 0xa5, 0x5D, 0xfE, 0xFE, 0x40},
     "BCDN ",
     15},
    {{0xdD, 0x6B, 0xf5, 0x6C, 0xA2, 0xad, 0xa2, 0x4c, 0x68, 0x3F,
      0xAC, 0x50, 0xE3, 0x77, 0x83, 0xe5, 0x5B, 0x57, 0xAF, 0x9F},
     "BNC ",
     12},
    {{0x1F, 0x57, 0x3D, 0x6F, 0xb3, 0xF1, 0x3d, 0x68, 0x9F, 0xF8,
      0x44, 0xB4, 0xcE, 0x37, 0x79, 0x4d, 0x79, 0xa7, 0xFF, 0x1C},
     "BNT ",
     18},
    {{0x5A, 0xf2, 0xBe, 0x19, 0x3a, 0x6A, 0xBC, 0xa9, 0xc8, 0x81,
      0x70, 0x01, 0xF4, 0x57, 0x44, 0x77, 0x7D, 0xb3, 0x07, 0x56},
     "BQX ",
     8},
    {{0x56, 0xba, 0x2E, 0xe7, 0x89, 0x04, 0x61, 0xf4, 0x63, 0xF7,
      0xbe, 0x02, 0xaA, 0xC3, 0x09, 0x9f, 0x6d, 0x58, 0x11, 0xA8},
     "CAT ",
     18},
    {{0x12, 0xFE, 0xF5, 0xe5, 0x7b, 0xF4, 0x58, 0x73, 0xCd, 0x9B,
      0x62, 0xE9, 0xDB, 0xd7, 0xBF, 0xb9, 0x9e, 0x32, 0xD7, 0x3e},
     "CFI ",
     18},
    {{0xAe, 0xf3, 0x8f, 0xBF, 0xBF, 0x93, 0x2D, 0x1A, 0xeF, 0x3B,
      0x80, 0x8B, 0xc8, 0xfB, 0xd8, 0xCd, 0x8E, 0x1f, 0x8B, 0xC5},
     "CRB ",
     8},
    {{0x4e, 0x06, 0x03, 0xe2, 0xa2, 0x7a, 0x30, 0x48, 0x0e, 0x5e,
      0x3a, 0x4f, 0xe5, 0x48, 0xe2, 0x9e, 0xf1, 0x2f, 0x64, 0xbe},
     "CRDO ",
     18},
    {{0xbf, 0x4c, 0xfd, 0x7d, 0x1e, 0xde, 0xee, 0xa5, 0xf6, 0x60,
      0x08, 0x27, 0x41, 0x1b, 0x41, 0xa2, 0x1e, 0xb0, 0x8a, 0xbd},
     "CTL ",
     2},
    {{0xE4, 0xc9, 0x4d, 0x45, 0xf7, 0xAe, 0xf7, 0x01, 0x8a, 0x5D,
      0x66, 0xf4, 0x4a, 0xF7, 0x80, 0xec, 0x60, 0x23, 0x37, 0x8e},
     "CCRB ",
     6},
    {{0x41, 0xe5, 0x56, 0x00, 0x54, 0x82, 0x4e, 0xa6, 0xb0, 0x73,
      0x2e, 0x65, 0x6e, 0x3a, 0xd6, 0x4e, 0x20, 0xe9, 0x4e, 0x45},
     "CVC ",
     8},
    {{0xBB, 0x9b, 0xc2, 0x44, 0xD7, 0x98, 0x12, 0x3f, 0xDe, 0x78,
      0x3f, 0xCc, 0x1C, 0x72, 0xd3, 0xBb, 0x8C, 0x18, 0x94, 0x13},
     "DAO ",
     16},
    {{0xcC, 0x4e, 0xF9, 0xEE, 0xAF, 0x65, 0x6a, 0xC1, 0xa2, 0xAb,
      0x88, 0x67, 0x43, 0xE9, 0x8e, 0x97, 0xE0, 0x90, 0xed, 0x38},
     "DDF ",
     18},
    {{0xE0, 0xB7, 0x92, 0x7c, 0x4a, 0xF2, 0x37, 0x65, 0xCb, 0x51,
      0x31, 0x4A, 0x0E, 0x05, 0x21, 0xA9, 0x64, 0x5F, 0x0E, 0x2A},
     "DGD ",
     9},
    {{0x55, 0xb9, 0xa1, 0x1c, 0x2e, 0x83, 0x51, 0xb4, 0xFf, 0xc7,
      0xb1, 0x15, 0x61, 0x14, 0x8b, 0xfa, 0xC9, 0x97, 0x78, 0x55},
     "DGX1 ",
     9},
    {{0x2e, 0x07, 0x1D, 0x29, 0x66, 0xAa, 0x7D, 0x8d, 0xEC, 0xB1,
      0x00, 0x58, 0x85, 0xbA, 0x19, 0x77, 0xD6, 0x03, 0x8A, 0x65},
     "DICE ",
     16},
    {{0x62, 0x1d, 0x78, 0xf2, 0xef, 0x2f, 0xd9, 0x37, 0xbf, 0xca,
      0x69, 0x6c, 0xab, 0xaf, 0x9a, 0x77, 0x9f, 0x59, 0xb3, 0xed},
     "DRP ",
     2},
    {{0x08, 0x71, 0x1D, 0x3B, 0x02, 0xC8, 0x75, 0x8F, 0x2F, 0xB3,
      0xab, 0x4e, 0x80, 0x22, 0x84, 0x18, 0xa7, 0xF8, 0xe3, 0x9c},
     "EDG ",
     0},
    {{0xB8, 0x02, 0xb2, 0x4E, 0x06, 0x37, 0xc2, 0xB8, 0x7D, 0x2E,
      0x8b, 0x77, 0x84, 0xC0, 0x55, 0xBB, 0xE9, 0x21, 0x01, 0x1a},
     "EMV ",
     2},
    {{0x86, 0xfa, 0x04, 0x98, 0x57, 0xe0, 0x20, 0x9a, 0xa7, 0xd9,
      0xe6, 0x16, 0xf7, 0xeb, 0x3b, 0x3b, 0x78, 0xec, 0xfd, 0xb0},
     "EOS ",
     18},
    {{0x19, 0x0e, 0x56, 0x9b, 0xE0, 0x71, 0xF4, 0x0c, 0x70, 0x4e,
      0x15, 0x82, 0x5F, 0x28, 0x54, 0x81, 0xCB, 0x74, 0xB6, 0xcC},
     "FAM ",
     12},
    {{0x41, 0x9D, 0x0d, 0x8B, 0xdD, 0x9a, 0xF5, 0xe6, 0x06, 0xAe,
      0x22, 0x32, 0xed, 0x28, 0x5A, 0xff, 0x19, 0x0E, 0x71, 0x1b},
     "FUN ",
     8},
    {{0x68, 0x10, 0xe7, 0x76, 0x88, 0x0C, 0x02, 0x93, 0x3D, 0x47,
      0xDB, 0x1b, 0x9f, 0xc0, 0x59, 0x08, 0xe5, 0x38, 0x6b, 0x96},
     "GNO ",
     18},
    {{0xa7, 0x44, 0x76, 0x44, 0x31, 0x19, 0xA9, 0x42, 0xdE, 0x49,
      0x85, 0x90, 0xFe, 0x1f, 0x24, 0x54, 0xd7, 0xD4, 0xaC, 0x0d},
     "GNT ",
     18},
    {{0xf7, 0xB0, 0x98, 0x29, 0x8f, 0x7C, 0x69, 0xFc, 0x14, 0x61,
      0x0b, 0xf7, 0x1d, 0x5e, 0x02, 0xc6, 0x07, 0x92, 0x89, 0x4C},
     "GUP ",
     3},
    {{0x1D, 0x92, 0x1E, 0xeD, 0x55, 0xa6, 0xa9, 0xcc, 0xaA, 0x9C,
      0x79, 0xB1, 0xA4, 0xf7, 0xB2, 0x55, 0x56, 0xe4, 0x43, 0x65},
     "GT ",
     0},
    {{0x14, 0xF3, 0x7B, 0x57, 0x42, 0x42, 0xD3, 0x66, 0x55, 0x8d,
      0xB6, 0x1f, 0x33, 0x35, 0x28, 0x9a, 0x50, 0x35, 0xc5, 0x06},
     "HKG ",
     3},
    {{0xcb, 0xCC, 0x0F, 0x03, 0x6E, 0xD4, 0x78, 0x8F, 0x63, 0xFC,
      0x0f, 0xEE, 0x32, 0x87, 0x3d, 0x6A, 0x74, 0x87, 0xb9, 0x08},
     "HMQ ",
     8},
    {{0x88, 0x86, 0x66, 0xCA, 0x69, 0xE0, 0xf1, 0x78, 0xDE, 0xD6,
      0xD7, 0x5b, 0x57, 0x26, 0xCe, 0xe9, 0x9A, 0x87, 0xD6, 0x98},
     "ICN ",
     18},
    {{0xc1, 0xE6, 0xC6, 0xC6, 0x81, 0xB2, 0x86, 0xFb, 0x50, 0x3B,
      0x36, 0xa9, 0xdD, 0x6c, 0x1d, 0xbF, 0xF8, 0x5E, 0x73, 0xCF},
     "JET ",
     18},
    {{0x77, 0x34, 0x50, 0x33, 0x5e, 0xD4, 0xec, 0x3D, 0xB4, 0x5a,
      0xF7, 0x4f, 0x34, 0xF2, 0xc8, 0x53, 0x48, 0x64, 0x5D, 0x39},
     "JTC ",
     18},
    {{0xfa, 0x05, 0xA7, 0x3F, 0xfE, 0x78, 0xef, 0x8f, 0x1a, 0x73,
      0x94, 0x73, 0xe4, 0x62, 0xc5, 0x4b, 0xae, 0x65, 0x67, 0xD9},
     "LUN ",
     18},
    {{0x93, 0xE6, 0x82, 0x10, 0x7d, 0x1E, 0x9d, 0xef, 0xB0, 0xb5,
      0xee, 0x70, 0x1C, 0x71, 0x70, 0x7a, 0x4B, 0x2E, 0x46, 0xBc},
     "MCAP ",
     8},
    {{0xB6, 0x3B, 0x60, 0x6A, 0xc8, 0x10, 0xa5, 0x2c, 0xCa, 0x15,
      0xe4, 0x4b, 0xB6, 0x30, 0xfd, 0x42, 0xD8, 0xd1, 0xd8, 0x3d},
     "MCO ",
     8},
    {{0x40, 0x39, 0x50, 0x44, 0xac, 0x3c, 0x0c, 0x57, 0x05, 0x19,
      0x06, 0xda, 0x93, 0x8b, 0x54, 0xbd, 0x65, 0x57, 0xf2, 0x12},
     "MGO ",
     8},
    {{0xd0, 0xb1, 0x71, 0xEb, 0x0b, 0x0F, 0x2C, 0xbD, 0x35, 0xcC,
      0xD9, 0x7c, 0xDC, 0x5E, 0xDC, 0x3f, 0xfe, 0x48, 0x71, 0xaa},
     "MDA ",
     18},
    {{0xe2, 0x3c, 0xd1, 0x60, 0x76, 0x1f, 0x63, 0xFC, 0x3a, 0x1c,
      0xF7, 0x8A, 0xa0, 0x34, 0xb6, 0xcd, 0xF9, 0x7d, 0x3E, 0x0C},
     "MIT ",
     18},
    {{0xC6, 0x6e, 0xA8, 0x02, 0x71, 0x7b, 0xFb, 0x98, 0x33, 0x40,
      0x02, 0x64, 0xDd, 0x12, 0xc2, 0xbC, 0xeA, 0xa3, 0x4a, 0x6d},
     "MKR ",
     18},
    {{0xBE, 0xB9, 0xeF, 0x51, 0x4a, 0x37, 0x9B, 0x99, 0x7e, 0x07,
      0x98, 0xFD, 0xcC, 0x90, 0x1E, 0xe4, 0x74, 0xB6, 0xD9, 0xA1},
     "MLN ",
     18},
    {{0x1a, 0x95, 0xB2, 0x71, 0xB0, 0x53, 0x5D, 0x15, 0xfa, 0x49,
      0x93, 0x2D, 0xab, 0xa3, 0x1B, 0xA6, 0x12, 0xb5, 0x29, 0x46},
     "MNE ",
     8},
    {{0x68, 0xAA, 0x3F, 0x23, 0x2d, 0xA9, 0xbd, 0xC2, 0x34, 0x34,
      0x65, 0x54, 0x57, 0x94, 0xef, 0x3e, 0xEa, 0x52, 0x09, 0xBD},
     "MSP ",
     18},
    {{0xf4, 0x33, 0x08, 0x93, 0x66, 0x89, 0x9d, 0x83, 0xa9, 0xf2,
      0x6a, 0x77, 0x3d, 0x59, 0xec, 0x7e, 0xcf, 0x30, 0x35, 0x5e},
     "MTL ",
     8},
    {{0xa6, 0x45, 0x26, 0x4C, 0x56, 0x03, 0xE9, 0x6c, 0x3b, 0x0B,
      0x07, 0x8c, 0xda, 0xb6, 0x87, 0x33, 0x79, 0x4B, 0x0A, 0x71},
     "MYST ",
     8},
    {{0xcf, 0xb9, 0x86, 0x37, 0xbc, 0xae, 0x43, 0xC1, 0x33, 0x23,
      0xEA, 0xa1, 0x73, 0x1c, 0xED, 0x2B, 0x71, 0x69, 0x62, 0xfD},
     "NET ",
     18},
    {{0x17, 0x76, 0xe1, 0xF2, 0x6f, 0x98, 0xb1, 0xA5, 0xdF, 0x9c,
      0xD3, 0x47, 0x95, 0x3a, 0x26, 0xdd, 0x3C, 0xb4, 0x66, 0x71},
     "NMR ",
     18},
    {{0x45, 0xe4, 0x2D, 0x65, 0x9D, 0x9f, 0x94, 0x66, 0xcD, 0x5D,
      0xF6, 0x22, 0x50, 0x60, 0x33, 0x14, 0x5a, 0x9b, 0x89, 0xBc},
     "NxC ",
     3},
    {{0x70, 0x1C, 0x24, 0x4b, 0x98, 0x8a, 0x51, 0x3c, 0x94, 0x59,
      0x73, 0xdE, 0xFA, 0x05, 0xde, 0x93, 0x3b, 0x23, 0xFe, 0x1D},
     "OAX ",
     18},
    {{0xd2, 0x61, 0x14, 0xcd, 0x6E, 0xE2, 0x89, 0xAc, 0xcF, 0x82,
      0x35, 0x0c, 0x8d, 0x84, 0x87, 0xfe, 0xdB, 0x8A, 0x0C, 0x07},
     "OMG ",
     18},
    {{0xB9, 0x70, 0x48, 0x62, 0x8D, 0xB6, 0xB6, 0x61, 0xD4, 0xC2,
      0xaA, 0x83, 0x3e, 0x95, 0xDb, 0xe1, 0xA9, 0x05, 0xB2, 0x80},
     "PAY ",
     18},
    {{0x0A, 0xfF, 0xa0, 0x6e, 0x7F, 0xbe, 0x5b, 0xC9, 0xa7, 0x64,
      0xC9, 0x79, 0xaA, 0x66, 0xE8, 0x25, 0x6A, 0x63, 0x1f, 0x02},
     "PLBT ",
     6},
    {{0x8A, 0xe4, 0xBF, 0x2C, 0x33, 0xa8, 0xe6, 0x67, 0xde, 0x34,
      0xB5, 0x49, 0x38, 0xB0, 0xcc, 0xD0, 0x3E, 0xb8, 0xCC, 0x06},
     "PTOY ",
     8},
    {{0xD8, 0x91, 0x2C, 0x10, 0x68, 0x1D, 0x8B, 0x21, 0xFd, 0x37,
      0x42, 0x24, 0x4f, 0x44, 0x65, 0x8d, 0xBA, 0x12, 0x26, 0x4E},
     "PLU ",
     18},
    {{0x67, 0x1A, 0xbB, 0xe5, 0xCE, 0x65, 0x24, 0x91, 0x98, 0x53,
      0x42, 0xe8, 0x54, 0x28, 0xEB, 0x1b, 0x07, 0xbC, 0x6c, 0x64},
     "QAU ",
     8},
    {{0x69, 0x7b, 0xea, 0xc2, 0x8B, 0x09, 0xE1, 0x22, 0xC4, 0x33,
      0x2D, 0x16, 0x39, 0x85, 0xe8, 0xa7, 0x31, 0x21, 0xb9, 0x7F},
     "QRL ",
     8},
    {{0xE9, 0x43, 0x27, 0xD0, 0x7F, 0xc1, 0x79, 0x07, 0xb4, 0xDB,
      0x78, 0x8E, 0x5a, 0xDf, 0x2e, 0xd4, 0x24, 0xad, 0xDf, 0xf6},
     "REP ",
     18},
    {{0x60, 0x7F, 0x4C, 0x5B, 0xB6, 0x72, 0x23, 0x0e, 0x86, 0x72,
      0x08, 0x55, 0x32, 0xf7, 0xe9, 0x01, 0x54, 0x4a, 0x73, 0x75},
     "RLC ",
     9},
    {{0xcC, 0xeD, 0x5B, 0x82, 0x88, 0x08, 0x6B, 0xE8, 0xc3, 0x8E,
      0x23, 0x56, 0x7e, 0x68, 0x4C, 0x37, 0x40, 0xbe, 0x4D, 0x48},
     "RLT ",
     10},
    {{0x49, 0x93, 0xCB, 0x95, 0xc7, 0x44, 0x3b, 0xdC, 0x06, 0x15,
      0x5c, 0x5f, 0x56, 0x88, 0xBe, 0x9D, 0x8f, 0x69, 0x99, 0xa5},
     "RND ",
     18},
    {{0xa1, 0xcc, 0xc1, 0x66, 0xfa, 0xf0, 0xE9, 0x98, 0xb3, 0xE3,
      0x32, 0x25, 0xA1, 0xA0, 0x30, 0x1B, 0x1C, 0x86, 0x11, 0x9D},
     "SGEL ",
     18},
    {{0xd2, 0x48, 0xB0, 0xD4, 0x8E, 0x44, 0xaa, 0xF9, 0xc4, 0x9a,
      0xea, 0x03, 0x12, 0xbe, 0x7E, 0x13, 0xa6, 0xdc, 0x14, 0x68},
     "SGT ",
     1},
    {{0xEF, 0x2E, 0x99, 0x66, 0xeb, 0x61, 0xBB, 0x49, 0x4E, 0x53,
      0x75, 0xd5, 0xDf, 0x8d, 0x67, 0xB7, 0xdB, 0x8A, 0x78, 0x0D},
     "SHIT ",
     0},
    {{0x2b, 0xDC, 0x0D, 0x42, 0x99, 0x60, 0x17, 0xfC, 0xe2, 0x14,
      0xb2, 0x16, 0x07, 0xa5, 0x15, 0xDA, 0x41, 0xA9, 0xE0, 0xC5},
     "SKIN ",
     6},
    {{0x49, 0x94, 0xe8, 0x18, 0x97, 0xa9, 0x20, 0xc0, 0xFE, 0xA2,
      0x35, 0xeb, 0x8C, 0xEd, 0xEE, 0xd3, 0xc6, 0xfF, 0xF6, 0x97},
     "SKO1 ",
     18},
    {{0xae, 0xC2, 0xE8, 0x7E, 0x0A, 0x23, 0x52, 0x66, 0xD9, 0xC5,
      0xAD, 0xc9, 0xDE, 0xb4, 0xb2, 0xE2, 0x9b, 0x54, 0xD0, 0x09},
     "SNGL ",
     0},
    {{0x98, 0x3F, 0x6d, 0x60, 0xdb, 0x79, 0xea, 0x8c, 0xA4, 0xeB,
      0x99, 0x68, 0xC6, 0xaF, 0xf8, 0xcf, 0xA0, 0x4B, 0x3c, 0x63},
     "SNM ",
     18},
    {{0x74, 0x4d, 0x70, 0xFD, 0xBE, 0x2B, 0xa4, 0xCF, 0x95, 0x13,
      0x16, 0x26, 0x61, 0x4a, 0x17, 0x63, 0xDF, 0x80, 0x5B, 0x9E},
     "SNT ",
     18},
    {{0x1d, 0xCE, 0x4F, 0xa0, 0x36, 0x39, 0xB7, 0xF0, 0xC3, 0x8e,
      0xe5, 0xbB, 0x60, 0x65, 0x04, 0x5E, 0xdC, 0xf9, 0x81, 0x9a},
     "SRC ",
     8},
    {{0xB6, 0x4e, 0xf5, 0x1C, 0x88, 0x89, 0x72, 0xc9, 0x08, 0xCF,
      0xac, 0xf5, 0x9B, 0x47, 0xC1, 0xAf, 0xBC, 0x0A, 0xb8, 0xaC},
     "STRJ ",
     8},
    {{0xB9, 0xe7, 0xF8, 0x56, 0x8e, 0x08, 0xd5, 0x65, 0x9f, 0x5D,
      0x29, 0xC4, 0x99, 0x71, 0x73, 0xd8, 0x4C, 0xdF, 0x26, 0x07},
     "SWT ",
     18},
    {{0xF4, 0x13, 0x41, 0x46, 0xAF, 0x2d, 0x51, 0x1D, 0xd5, 0xEA,
      0x8c, 0xDB, 0x1C, 0x4A, 0xC8, 0x8C, 0x57, 0xD6, 0x04, 0x04},
     "SNC ",
     18},
    {{0xE7, 0x77, 0x5A, 0x6e, 0x9B, 0xcf, 0x90, 0x4e, 0xb3, 0x9D,
      0xA2, 0xb6, 0x8c, 0x5e, 0xfb, 0x4F, 0x93, 0x60, 0xe0, 0x8C},
     "TaaS ",
     6},
    {{0xa7, 0xf9, 0x76, 0xC3, 0x60, 0xeb, 0xBe, 0xD4, 0x46, 0x5c,
      0x28, 0x55, 0x68, 0x4D, 0x1A, 0xAE, 0x52, 0x71, 0xeF, 0xa9},
     "TFL ",
     8},
    {{0x65, 0x31, 0xf1, 0x33, 0xe6, 0xDe, 0xeB, 0xe7, 0xF2, 0xdc,
      0xE5, 0xA0, 0x44, 0x1a, 0xA7, 0xef, 0x33, 0x0B, 0x4e, 0x53},
     "TIME ",
     8},
    {{0xEa, 0x1f, 0x34, 0x6f, 0xaF, 0x02, 0x3F, 0x97, 0x4E, 0xb5,
      0xad, 0xaf, 0x08, 0x8B, 0xbC, 0xdf, 0x02, 0xd7, 0x61, 0xF4},
     "TIX ",
     18},
    {{0xaA, 0xAf, 0x91, 0xD9, 0xb9, 0x0d, 0xF8, 0x00, 0xDf, 0x4F,
      0x55, 0xc2, 0x05, 0xfd, 0x69, 0x89, 0xc9, 0x77, 0xE7, 0x3a},
     "TKN ",
     8},
    {{0xCb, 0x94, 0xbe, 0x6f, 0x13, 0xA1, 0x18, 0x2E, 0x4A, 0x4B,
      0x61, 0x40, 0xcb, 0x7b, 0xf2, 0x02, 0x5d, 0x28, 0xe4, 0x1B},
     "TRST ",
     6},
    {{0x89, 0x20, 0x5A, 0x3A, 0x3b, 0x2A, 0x69, 0xDe, 0x6D, 0xbf,
      0x7f, 0x01, 0xED, 0x13, 0xB2, 0x10, 0x8B, 0x2c, 0x43, 0xe7},
     "o> ",
     0},
    {{0x5c, 0x54, 0x3e, 0x7A, 0xE0, 0xA1, 0x10, 0x4f, 0x78, 0x40,
      0x6C, 0x34, 0x0E, 0x9C, 0x64, 0xFD, 0x9f, 0xCE, 0x51, 0x70},
     "VSL ",
     18},
    {{0x82, 0x66, 0x57, 0x64, 0xea, 0x0b, 0x58, 0x15, 0x7E, 0x1e,
      0x5E, 0x9b, 0xab, 0x32, 0xF6, 0x8c, 0x76, 0xEc, 0x0C, 0xdF},
     "VSM ",
     0},
    {{0x8f, 0x34, 0x70, 0xA7, 0x38, 0x8c, 0x05, 0xeE, 0x4e, 0x7A,
      0xF3, 0xd0, 0x1D, 0x8C, 0x72, 0x2b, 0x0F, 0xF5, 0x23, 0x74},
     "VERI ",
     18},
    {{0xeD, 0xBa, 0xF3, 0xc5, 0x10, 0x03, 0x02, 0xdC, 0xdd, 0xA5,
      0x32, 0x69, 0x32, 0x2f, 0x37, 0x30, 0xb1, 0xF0, 0x41, 0x6d},
     "VRS ",
     5},
    {{0x66, 0x70, 0x88, 0xb2, 0x12, 0xce, 0x3d, 0x06, 0xa1, 0xb5,
      0x53, 0xa7, 0x22, 0x1E, 0x1f, 0xD1, 0x90, 0x00, 0xd9, 0xaF},
     "WING ",
     18},
    {{0x4D, 0xF8, 0x12, 0xF6, 0x06, 0x4d, 0xef, 0x1e, 0x5e, 0x02,
      0x9f, 0x1c, 0xa8, 0x58, 0x77, 0x7C, 0xC9, 0x8D, 0x2D, 0x81},
     "XAUR ",
     8},
    {{0xB1, 0x10, 0xeC, 0x7B, 0x1d, 0xcb, 0x8F, 0xAB, 0x8d, 0xED,
      0xbf, 0x28, 0xf5, 0x3B, 0xc6, 0x3e, 0xA5, 0xBE, 0xdd, 0x84},
     "XID ",
     8},

};
