/* ASM8, Intel 8008 Assembler
 * By Yasin Morsli
 * Assembles Program Code for Intel 8008 Microprocessors
 * Uses new MNEMONICS
*/

#ifndef MAIN_H
#define MAIN_H

using namespace std;

#include <iostream>
#include <string>

int error(string _str);

const char info[] = "ASM8 Intel 8008 Assembler, by Yasin\n"
                    "asm8 [-options] \"in.asm\"\n"
                    "type option -h for help\n";

const char help[] = "  -h show this help\n"
                    "  -i specify input file\n"
                    "  -o specify output file\n"
                    "  use \"\" if path contains spaces";

string opt_i, opt_o;

int main(int argc, char** argv);

#endif
