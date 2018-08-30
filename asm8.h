/* ASM8, Intel 8008 Assembler
 * By Yasin Morsli
 * Assembles Program Code for Intel 8008 Microprocessors
 * Uses new MNEMONICS
*/

#ifndef ASM8_H_
#define ASM8_H_

using namespace std;

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#define num_elements(a)     ((uint32_t)(sizeof(a) / sizeof(a[0])))

#define ERROR_INVALID_EXPRESSION            -0
#define ERROR_OPEN_FILE                     -1
#define ERROR_CREATE_FILE                   -2
#define ERROR_FILE_STACK_OVERFLOW           -3
#define ERROR_DEFINE_MACRO_ALREADY_EXISTS   -4
#define ERROR_INVALID_LABEL                 -5
#define ERROR_LABEL_ALREADY_EXISTS          -6
#define ERROR_ENDM_NOT_FOUND                -7
#define ERROR_MISSING_MACRO_ARGS            -8
#define ERROR_MAX_TRIES_PASS_2              -9
#define ERROR_INVALID_INSTR                 -10
#define ERROR_ORG_BACK                      -11

#define MAX_FILE_STACK_SIZE         100
#define MAX_TRIES_PASS_2            100

class file_t{
    public:
        string      filename;
        string      content;
        uint32_t    total_lines;
        uint32_t    current_line;
        
        file_t(string _filename);
        void filename_convert_slash();
        void content_convert_newline();
};

class define_t{
    public:
        string      name;
        int         value;
        string      details_first_defined;
        
        define_t(string _name, int _value, string _details = "");
};

class macro_t{
    public:
        string  name;
        vector<string>          arg_list;
        vector<vector<string>>  code;
        uint32_t insert_count;
        string  details_first_defined;
        
        macro_t(string _name, vector<string> _arg_list, vector<vector<string>> _code, string _details = "");
};

class label_t{
    public:
        string      name;
        uint32_t    address;
        string details_first_defined;
        
        label_t(string _name, string _details = "");
};

class asm8{
    public:
        enum instr_type {
            add_instr, adc_instr, sub_instr, sbc_instr,
            and_instr, xor_instr, or_instr,  cp_instr,
            halt_instr, nop_instr, ld_instr,
            rlc_instr, ral_instr, rrc_instr, rar_instr,
            ret_instr, rst_instr, jmp_instr, cal_instr,
            inp_instr, out_instr, inc_instr, dec_instr,
            empty_instr, invalid_instr
        };
        
        static asm8         *asm8_p;
        vector<file_t>      file_stack;
        file_t              *current_file;
        vector<vector<string>>  code;
        uint32_t            code_line;
        vector<define_t>    define_list;
        vector<macro_t>     macro_list;
        vector<label_t>     label_list;
        
        vector<uint8_t>      binary_out;
        uint32_t             address;
        
        asm8();
        int assemble(string _in, string _out);
        void open_file(string _filename);
        void close_current_file();
        
        const string special_char = ",()<>+-*/";
        vector<string> split_current_line_into_words();
        
        string add_label(vector<string> _code);
        
        bool is_directive(vector<string> _code, uint32_t _directive);
        bool is_label(vector<string> _code);
        bool is_define(vector<string> _code);
        bool is_macro(vector<string> _code);
        
        bool contains_define(vector<string> _code);
        bool contains_macro(vector<string> _code);
        bool contains_label(vector<string> _code);
        bool contains_expression(vector<string> _code);
        
        vector<string> insert_define(vector<string> _code);
        vector<vector<string>> insert_macro(vector<string> _code);
        vector<string> insert_label(vector<string> _code);
        vector<string>evaluate_expression(vector<string> _code);
        
        int32_t string_to_num(string _str);
        
        enum directives_t {include_d = 0,   define_d = 1,   macro_d = 2,    endm_d = 3,
                           if_d = 4,        endif_d = 5,    org_d = 6,      base_d = 7,
                           db_d = 8,        dsb_d = 9,      incbin_d = 10,
                           any_d = 15
                          };
        const string directives[11] = {
            ".include",
            ".define",
            ".macro",
            ".endm",
            ".if",
            ".endif",
            ".org",
            ".base",
            ".db",
            ".dsb",
            ".incbin"
        };
        void do_directive(vector<string> _code);
        string directive_include(vector<string> _code);
        string directive_define(vector<string> _code);
        string directive_macro(vector<string> _code);
        string directive_endm(vector<string> _code);
        string directive_if(vector<string> _code);
        string directive_endif(vector<string> _code);
        string directive_org(vector<string> _code);
        string directive_base(vector<string> _code);
        string directive_dsb(vector<string> _code, uint32_t _addr);
        string directive_incbin(vector<string> _code);
        void skip_directive_macro(vector<string> _code);
        void skip_directive_if(vector<string> _code);
        
        uint32_t get_db_size(vector<string> _code);
        uint32_t get_dsb_size(vector<string> _code);
        
        int32_t get_label_id(vector<string> _code);
        void set_label_addr(uint32_t _label_id, uint32_t _addr);
        int get_define_value(vector<string> _code);
        string get_macro(vector<string> _code);
        
        instr_type get_instr_type(vector<string> _code);
        bool check_instr_valid(vector<string> _code);
        uint8_t get_instr_size(vector<string> _code);
        vector<uint8_t> translate_instr(vector<string> _code);
        
        static int error(int _error, string _str = "");
};

#endif /* ASM8_H_ */
