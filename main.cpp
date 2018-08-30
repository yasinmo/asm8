/* ASM8, Intel 8008 Assembler
 * By Yasin Morsli
 * Assembles Program Code for Intel 8008 Microprocessors
 * Uses new MNEMONICS
*/

using namespace std;

#include <iostream>
#include <fstream>
#include <string>
#include "main.h"
#include "asm8.h"

int main(int argc, char** argv){
    cout << info << endl;
    if(argc <= 1){ cerr << "No input file specified..." << endl; return -1;}
    
    //Clear option fields:
    opt_i = ""; opt_o = "";
    //Parse options:
    if(string(argv[1]) == string("-h")) {cout << help << endl; return 0;}
    if(argc == 2) opt_i = argv[1];
    else{
        for(int i = 1; i < argc; i++){
            if(argv[i] == string("-i"))         opt_i = argv[i + 1];
            else if(argv[i] == string("-o"))    opt_o = argv[i + 1];
        }
    }
    
    //If -o was omitted, then default is "ifile_name.bin":
    if(opt_o == "") {opt_o = (opt_i.rfind('.') == string::npos) ? opt_i + "" : opt_i.substr(0, opt_i.rfind('.')) + ".bin";}

    return asm8().assemble(opt_i, opt_o);
}

