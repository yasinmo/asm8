/* ASM8, Intel 8008 Assembler
 * By Yasin Morsli
 * Assembles Program Code for Intel 8008 Microprocessors
 * Uses new MNEMONICS
*/

using namespace std;

#include <iostream>
#include <fstream>
#include <vector>
#include "asm8.h"

asm8 *asm8::asm8_p;

file_t::file_t(string _filename){
    filename = _filename;
    content = "";
    current_line = 0;
    total_lines = 0;
    
    filename_convert_slash();
    
    ifstream _file(filename);
    if(!_file.is_open()) asm8::error(ERROR_OPEN_FILE, filename);
    while(_file.peek() != EOF) content += tolower(_file.get());
    _file.close();
    content_convert_newline();
    
    for(uint32_t i = 0; i < content.length(); i++) if(content[i] == '\n') total_lines++;
}
void file_t::filename_convert_slash(){
    uint32_t i;
    while((i = filename.find("\\")) != string::npos) filename[i] = '/';
}
void file_t::content_convert_newline(){
    uint32_t i;
    while((i = content.find("\r\n")) != string::npos) content[i] = '\n';
    while((i = content.find("\r")) != string::npos) content[i] = '\n';
}

define_t::define_t(string _name, int _value, string _details){
    name = _name;
    value = _value;
    details_first_defined = _details;
}

macro_t::macro_t(string _name, vector<string> _arg_list, vector<vector<string>> _code, string _details){
    name = _name;
    arg_list = _arg_list;
    code = _code;
    insert_count = 0;
    details_first_defined = _details;
}

label_t::label_t(string _name, string _details){
    name = _name;
    details_first_defined = _details;
}

asm8::asm8(){
    asm8_p = this;
    file_stack.clear();
    current_file = nullptr;
    code.clear();
    code_line = 0;
    define_list.clear();
    macro_list.clear();
    label_list.clear();
    binary_out.clear();
    address = 0;
}

int asm8::assemble(string _in, string _out){
    open_file(_in);
    
    //Pass 1: Search for defines, macros and labels:
    while(file_stack.size() > 0){
        while(current_file->current_line <= current_file->total_lines){
            vector<string> _code_line = split_current_line_into_words();
            current_file->current_line++;
            if(_code_line.size() > 0){
                if(is_directive(_code_line, include_d)){
                    directive_include(_code_line);
                }
                else if(is_directive(_code_line, define_d)){
                    directive_define(_code_line);
                }
                else if(is_directive(_code_line, macro_d)){
                    directive_macro(_code_line);
                }
                else if(is_label(_code_line)){
                    add_label(_code_line);
                }
            }
        }
        close_current_file();
        if(file_stack.size() > 0){
            current_file = &file_stack.back();
        }
    }
    
    //DEBUG:
    cout << "--------------------------------" << endl;
    cout << "Pass 1:" << 
            "\n\tLabels: \t" << label_list.size() <<
            "\n\tDefines:\t" << define_list.size() <<
            "\n\tMacros: \t" << macro_list.size() << endl;
    cout << "--------------------------------" << endl;
    
    //Pass 2: insert defines and macros
    uint32_t _pass_2_address = 0;
    uint32_t _pass_2_label_pc = 0;
    uint32_t _tries_pass_2 = 0;
    open_file(_in);
    while(file_stack.size() > 0){
        vector<vector<string>> _code;
        int32_t _cline = 0;
        while(current_file->current_line <= current_file->total_lines){
            _code.clear();
            _cline = 0;
            _code.push_back(split_current_line_into_words());
            _tries_pass_2 = 0;
            current_file->current_line++;
            while(_cline < _code.size()){
                if((_code[_cline].size() > 0)){
                    if(is_directive(_code[_cline], include_d)){
                        directive_include(_code[_cline]);
                        _code[_cline].clear();
                        _code[_cline].push_back("");
                    }
                    else if(is_directive(_code[_cline], define_d)){
                        _code[_cline] = split_current_line_into_words();
                    }
                    else if(is_directive(_code[_cline], macro_d)){
                        skip_directive_macro(_code[_cline]);
                        _code[_cline] = split_current_line_into_words();
                    }
                    else if(is_directive(_code[_cline], base_d)){
                        _pass_2_label_pc = string_to_num(_code[_cline][1]);
                    }
                    else if(is_directive(_code[_cline], org_d)){
                        uint32_t _pass_2_org = string_to_num(_code[_cline][1]);
                        if(_pass_2_org < _pass_2_address) error(ERROR_ORG_BACK);
                        _pass_2_address = _pass_2_org;
                        _pass_2_label_pc = _pass_2_org;
                    }
                    else if(is_directive(_code[_cline], db_d)){
                        _pass_2_address += get_db_size(_code[_cline]);
                        _pass_2_label_pc += get_db_size(_code[_cline]);
                    }
                    else if(is_directive(_code[_cline], dsb_d)){
                        directive_dsb(_code[_cline], _pass_2_label_pc);
//                        _pass_2_address += get_dsb_size(_code[_cline]);
                        _pass_2_label_pc += get_dsb_size(_code[_cline]);
                    }
                    
                    if(is_label(_code[_cline])){
                        if(get_label_id(_code[_cline]) < 0) add_label(_code[_cline]);
                        set_label_addr(get_label_id(_code[_cline]), _pass_2_label_pc);
                    }
                    
                    if(contains_define(_code[_cline]) || contains_macro(_code[_cline]) || contains_label(_code[_cline])){
                        if(contains_define(_code[_cline])){
                            _code[_cline] = insert_define(_code[_cline]);
                            continue;
                        }
                        else if(contains_macro(_code[_cline])){
                            vector<vector<string>> _macro_code = insert_macro(_code[_cline]);
                            _code[_cline] = _macro_code[0];
                            for(uint32_t i = 1; i < _macro_code.size(); i++)
                                _code.insert(_code.begin() + _cline + i, _macro_code[i]);
                            continue;
                        }
                    }
                }
                if(check_instr_valid(_code[_cline])){
                    _pass_2_address += get_instr_size(_code[_cline]);
                    _pass_2_label_pc += get_instr_size(_code[_cline]);
                }
                
                if(_cline < _code.size()) _cline++;
                
                if(_tries_pass_2 >= MAX_TRIES_PASS_2)   error(ERROR_MAX_TRIES_PASS_2);
                else                                    _tries_pass_2++;
            }
            
            for(uint32_t i = 0; i < _code.size(); i++){
                if(!(is_directive(_code[i], any_d)) && (_code[i].size() > 0)){
//                    //DEBUG
//                    for(uint32_t j = 0; j < _code[i].size(); j++) cout << _code[i][j] << " ";
//                    cout << endl;
                    
                    if(!check_instr_valid(_code[i])) error(ERROR_INVALID_INSTR);
                }
//                //DEBUG
//                else{
//                    cout << endl;
//                }
            }
            
//            //DEBUG
//            for(uint32_t i = 0; i < _code.size(); i++){
//                for(uint32_t j = 0; j < _code[i].size(); j++) cout << _code[i][j] << " ";
//                cout << endl;
//            }
            
            for(uint32_t i = 0; i < _code.size(); i++){
                code.push_back(_code[i]);
            }
        }
        close_current_file();
        if(file_stack.size() > 0){
            current_file = &file_stack.back();
        }
    }
    
//    //DEBUG
//    for(uint32_t i = 0; i < code.size(); i++){
//        for(uint32_t j = 0; j < code[i].size(); j++)
//            cout << code[i][j] << " ";
//        cout << endl;
//    }
    
    //Pass 3: Assemble code
    uint32_t _pass_3_label_pc = 0;
    uint32_t _last_pass_3_label_pc = -1;
    vector<uint8_t> _translated_code;
    address = 0;
    for(uint32_t i = 0; i < code.size(); i++){
        //DEBUG
        if(code[i].size() > 0){
            char _buf[32];
            sprintf(_buf, "0x%04X:\t", address);
            cout << _buf;
            
            for(uint32_t j = 0; j < code[i].size(); j++) cout << code[i][j] << " ";
            
            cout << endl;
        
            _last_pass_3_label_pc = _pass_3_label_pc;
            if(check_instr_valid(code[i])) _pass_3_label_pc += get_instr_size(code[i]);
            
            if(contains_label(code[i])){
                code[i] = insert_label(code[i]);
            }
            if(contains_expression(code[i])){
                code[i] = evaluate_expression(code[i]);
            }
            _translated_code = translate_instr(code[i]);
            for(uint32_t j = 0; j < _translated_code.size(); j++){
                binary_out.push_back(_translated_code[j]);
                address++;
            }
        }
    }
    
    ofstream ofile;
    ofile.open(_out.c_str(), ios::out | ios::binary | ios::trunc);
    if(!ofile.is_open()) return error(ERROR_CREATE_FILE, _out);
    for(uint32_t i = 0; i < binary_out.size(); i++) ofile.put(binary_out[i]);
    ofile.close();
    
    cout << "\nFile successfully assembled\n" << "File saved to: " << _out << endl;
    
    return 0;
}

void asm8::open_file(string _filename){
    if(file_stack.size() > MAX_FILE_STACK_SIZE) error(ERROR_FILE_STACK_OVERFLOW);
    file_t _file(_filename);
    file_stack.push_back(_file);
    current_file = &file_stack.back();
}

void asm8::close_current_file(){
    if(file_stack.size() > 0) file_stack.pop_back();
}

vector<string> asm8::split_current_line_into_words(){
    vector<string> _code;
    if(current_file->current_line > current_file->total_lines) return _code;
    uint32_t _cursor = 0;
    for(uint32_t i = 0; i < current_file->current_line; i++){
        if(current_file->content.find('\n', _cursor) == string::npos) break;
        else _cursor = current_file->content.find('\n', _cursor) + 1;
    }
    uint32_t _len = (current_file->content.find('\n', _cursor) == string::npos) ? string::npos : current_file->content.find('\n', _cursor) - _cursor;
    string _line = current_file->content.substr(_cursor, _len);
    uint32_t _comment_begin = (_line.find(';') == string::npos) ? string::npos : _line.find(';');
    _line = _line.substr(0, _comment_begin);
    
    string _word = "";
    bool _was_special_char = false;
    for(uint32_t i = 0; i <= _line.length(); i++){
        _was_special_char = false;
        for(uint32_t j = 0; j < special_char.length(); j++){
            if(_line[i] == special_char[j]){
                if(_word.length() > 0) _code.push_back(_word);
                _code.push_back(special_char.substr(j, 1));
                _word = "";
                _was_special_char = true;
            }
        }
        if(_was_special_char) continue;
        if(!isspace(_line[i]) && !(_line[i] == '\0')){
            _word += _line[i];
        }
        else if(_word.length() > 0){
            _code.push_back(_word);
            _word = "";
        }
    }
    return _code;
}

string asm8::add_label(vector<string> _code){
    if((_code.size() > 0)){
        if(_code[0].length() < 2) error(ERROR_INVALID_LABEL);
        if(!(isalpha(_code[0][0]) || (_code[0][0] == '_'))) error(ERROR_INVALID_LABEL);
        string _label_name = _code[0].substr(0, _code[0].find(":"));
        for(uint32_t i = 0; i < label_list.size(); i++)
            if(_label_name == label_list[i].name) error(ERROR_LABEL_ALREADY_EXISTS, _label_name + "\nfirst defined here: " + label_list[i].details_first_defined);
        label_t _label(_label_name, current_file->filename + ", Line " + to_string(current_file->current_line));
        label_list.push_back(_label);
    }
    return "";
}

bool asm8::is_directive(vector<string> _code, uint32_t _directive){
    if(_code.size() < 1) return false;
    if(_directive == any_d) return (_code[0][0] == '.');
    else                    return (_code[0] == directives[_directive]);
}

bool asm8::is_label(vector<string> _code){
    if(_code.size() < 1) return false;
    return (_code[0].find(":") != string::npos);
}

bool asm8::is_define(vector<string> _code){
    if(_code.size() < 1) return false;
    bool _is_define = false;
    for(uint32_t i = 0; i < _code.size(); i++){
        for(uint32_t j = 0; j < define_list.size(); j++){
            _is_define |= (_code[i] == define_list[j].name);
        }
    }
    return _is_define;
}

bool asm8::is_macro(vector<string> _code){
    if(_code.size() < 1) return false;
    bool _is_macro = false;
    for(uint32_t i = 0; i < _code.size(); i++){
        for(uint32_t j = 0; j < macro_list.size(); j++){
            _is_macro |= (_code[i] == macro_list[j].name);
        }
    }
    return _is_macro;
}

bool asm8::contains_define(vector<string> _code){
    if(_code.size() < 1) return false;
    for(uint32_t i = 0; i < _code.size(); i++){
        for(uint32_t j = 0; j < define_list.size(); j++){
            if(_code[i] == define_list[j].name){
                return true;
            }
        }
    }
    return false;
}

bool asm8::contains_macro(vector<string> _code){
    if(_code.size() < 1) return false;
    for(uint32_t i = 0; i < _code.size(); i++){
        for(uint32_t j = 0; j < macro_list.size(); j++){
            if(_code[i] == macro_list[j].name){
                return true;
            }
        }
    }
    return false;
}

bool asm8::contains_label(vector<string> _code){
    if(_code.size() < 1) return false;
    for(uint32_t i = 0; i < _code.size(); i++){
        for(uint32_t j = 0; j < label_list.size(); j++){
            if(_code[i] == label_list[j].name){
                return true;
            }
        }
    }
    return false;
}

bool asm8::contains_expression(vector<string> _code){
    if(_code.size() < 2) return false;
    for(uint32_t i = 0; i < _code.size(); i++) if(_code[i] == "(") return true;
    return false;
}

vector<string> asm8::insert_define(vector<string> _code){
    if(_code.size() < 1) return _code;
    for(uint32_t i = 0; i < _code.size(); i++){
        for(uint32_t j = 0; j < define_list.size(); j++){
            if(_code[i] == define_list[j].name){
                _code[i] = to_string(define_list[j].value);
            }
        }
    }
    
//    //DEBUG
//    for(uint32_t i = 0; i < _code.size(); i++) cout << _code[i] << " ";
//    cout << endl;
    
    return _code;
}

vector<vector<string>> asm8::insert_macro(vector<string> _code){
    vector<vector<string>> _macro_code;
    vector<string> _macro_code_line;
    vector<string> _arg_list;
    string _arg;
    uint32_t _macro_id;
    
    if(_code.size() < 1) return _macro_code;
    for(uint32_t i = 0; i < macro_list.size(); i++){
        if(_code[0] == macro_list[i].name){
            _macro_id = i; break;
        }
    }
    
    _arg = "";
    for(uint32_t i = 1; i < _code.size(); i++){
        if(_code[i] == ","){
            if(_arg.size() > 0) _arg_list.push_back(_arg);
            _arg = "";
        }
        else{
            _arg += _code[i];
            if((i + 1) == _code.size()) if(_arg.size() > 0) _arg_list.push_back(_arg);
        }
    }
    if(_arg_list.size() != macro_list[_macro_id].arg_list.size()) error(ERROR_MISSING_MACRO_ARGS);
    
    for(uint32_t _mline = 0; _mline < macro_list[_macro_id].code.size(); _mline++){
        _macro_code_line.clear();
        for(uint32_t _mword = 0; _mword < macro_list[_macro_id].code[_mline].size(); _mword++){
            _macro_code_line.push_back(macro_list[_macro_id].code[_mline][_mword]);
            for(uint32_t i = 0; i < macro_list[_macro_id].arg_list.size(); i++){
                if(_macro_code_line[_mword] == macro_list[_macro_id].arg_list[i]){
                    _macro_code_line[_mword] = _arg_list[i];
                }
            }
        }
        _macro_code.push_back(_macro_code_line);
    }
    
    vector<string> _macro_label;
    for(uint32_t i = 0; i < macro_list[_macro_id].code.size(); i++){
        if(is_label(_macro_code[i])){
            _macro_label.push_back(_macro_code[i][0].substr(0, _macro_code[i][0].find(":")));
            _macro_code[i][0].insert(_macro_code[i][0].find(":"), string("_") + to_string(macro_list[_macro_id].insert_count));
        }
    }
    for(uint32_t _mline = 0; _mline < macro_list[_macro_id].code.size(); _mline++){
        for(uint32_t _mword = 0; _mword < macro_list[_macro_id].code[_mline].size(); _mword++){
            for(uint32_t i = 0; i < _macro_label.size(); i++){
                if(_macro_code[_mline][_mword] == _macro_label[i]){
                    _macro_code[_mline][_mword] = _macro_label[i] + (string("_") + to_string(macro_list[_macro_id].insert_count));
                }
            }
        }
    }
    
    macro_list[_macro_id].insert_count++;
    
//    //DEBUG
//    for(uint32_t i = 0; i < _macro_code.size(); i++){
//        for(uint32_t j = 0; j < _macro_code[i].size(); j++){
//            cout << _macro_code[i][j] << " ";
//        }
//        cout << endl;
//    }
//    cout << endl;
    
    return _macro_code;
}

vector<string> asm8::insert_label(vector<string> _code){
    if(_code.size() < 1) return _code;
    for(uint32_t i = 0; i < label_list.size(); i++){
        for(uint32_t j = 0; j < _code.size(); j++){
            if(_code[j].substr(0, _code[j].find(":")) == label_list[i].name){
                _code[j] = to_string(label_list[i].address);
                return _code;
            }
        }
    }
}

vector<string> asm8::evaluate_expression(vector<string> _code){
    if(_code.size() < 2) return _code;
    
    int32_t _result = 0;
    string _result_str;
    string _operation;
    
    uint32_t _exp_start = 0;
    vector<string> _exp;
    string _word = "";
    bool _was_special_char = false;
    for(uint32_t i = 0; i < _code.size(); i++){
        for(uint32_t j = 0; j <= _code[i].size(); j++){
            _was_special_char = false;
            for(uint32_t k = 0; k < special_char.length(); k++){
                if(_code[i][j] == special_char[k]){
                    if(_word.length() > 0) _exp.push_back(_word);
                    _exp.push_back(special_char.substr(k, 1));
                    _word = "";
                    _was_special_char = true;
                }
            }
            if(_was_special_char) continue;
            if(!isspace(_code[i][j]) && !(_code[i][j] == '\0')){
                _word += _code[i][j];
            }
            else if(_word.length() > 0){
                _exp.push_back(_word);
                _word = "";
            }
        }
    }
    _code = _exp;
    
    uint32_t _tries = 0;
    while(contains_define(_code) || contains_label(_code)){
        if(contains_define(_code)){
            _code = insert_define(_code);
        }
        else if(contains_label(_code)){
            _code = insert_label(_code);
        }
        _tries++;
        if(_tries > MAX_TRIES_PASS_2) error(ERROR_MAX_TRIES_PASS_2);
    }
    
    _result = 0;
    for(uint32_t i = 0; i < _code.size(); i++) if(_code[i] == "("){_exp_start = i + 1; break;}
    for(uint32_t i = _exp_start; i < _code.size(); i++){
        if(isdigit(_code[i][0])){
            if(_operation == "+")       _result += string_to_num(_code[i]);
            else if(_operation == "-")  _result -= string_to_num(_code[i]);
            else if(_operation == "<")  _result += (string_to_num(_code[i]) & 0xFF);
            else if(_operation == ">")  _result += ((string_to_num(_code[i]) >> 8) & 0xFF);
            else                        _result += string_to_num(_code[i]);
        }
        else if(_code[i] == "("){
            vector<string> _new_exp;
            bool _bracket_found = false;
            for(uint32_t x = 0; x < _code.size(); x++){
                _new_exp.push_back(_code[i]);
                _code.erase(_code.begin() + i);
                if(_new_exp[x] == ")"){
                    _bracket_found = true;
                    break;
                }
            }
            if(!_bracket_found) error(ERROR_INVALID_EXPRESSION);
            _new_exp = evaluate_expression(_new_exp);
            for(uint32_t x = 0; x < _new_exp.size(); x++) _code.insert(_code.begin() + i, _new_exp[x]);
            i--;
            continue;
        }
        else if(_code[i].find_first_of("+-<>") != string::npos){
            _operation = _code[i];
            continue;
        }
        else{
            continue;
        }
    }
    
//    if(_code[2] == "<"){
//        _result_str = to_string(_result & 0xFF);
//    }
//    else if(_code[2] == ">"){
//        _result_str = to_string((_result >> 8) & 0xFF);
//    }
//    else{
//        _result_str = to_string(_result);
//    }
    _result_str = to_string(_result);
    _code.resize(_exp_start - 1);
    _code.push_back(_result_str);
    
    return _code;
}

int32_t asm8::string_to_num(string _str){
    if(_str.find("\'") != string::npos) return _str.substr(_str.find("\'") + 1, 1)[0];
    else{
        try{
            if(_str.find("0x") != string::npos) return stoi(_str.substr(_str.find("0x")), nullptr, 16);
            else                                return stoi(_str, nullptr, 10);
        } catch(invalid_argument){error(ERROR_INVALID_EXPRESSION);}
    }
    error(ERROR_INVALID_EXPRESSION);
}

void asm8::do_directive(vector<string> _code){
    if(_code.size() < 1) return;
    if(_code[0].find(directives[include_d]) != string::npos)
        directive_include(_code);
    else if(_code[0].find(directives[define_d]) != string::npos)
        directive_define(_code);
    else if(_code[0].find(directives[macro_d]) != string::npos)
        directive_macro(_code);
    else if(_code[0].find(directives[endm_d]) != string::npos)
        directive_endm(_code);
    else if(_code[0].find(directives[if_d]) != string::npos)
        directive_if(_code);
    else if(_code[0].find(directives[endif_d]) != string::npos)
        directive_endif(_code);
}

string asm8::directive_include(vector<string> _code){
    if((_code.size() > 1) && (_code[1].length() > 2)){
        uint32_t _pathlen = current_file->filename.rfind("/");
        string _filename = (_pathlen != string::npos) ? current_file->filename.substr(0, _pathlen + 1) : "";
        _filename += _code[1].substr(_code[1].find('\"') + 1, (_code[1].rfind('\"') - (_code[1].find('\"') + 1)));
        
        open_file(_filename);
        
        return _code[1];
    }
    else error(ERROR_INVALID_EXPRESSION);
    return "";
}

string asm8::directive_define(vector<string> _code){
    int _value = 0;
    _value = string_to_num(_code[2]);
    if((_code.size() > 1) && (_code[1].size() > 0)){
        for(uint32_t i = 0; i < define_list.size(); i++)
            if(_code[1] == define_list[i].name) error(ERROR_DEFINE_MACRO_ALREADY_EXISTS, _code[1] + "\nfirst defined here: " + define_list[i].details_first_defined);
        for(uint32_t i = 0; i < macro_list.size(); i++)
            if(_code[1] == macro_list[i].name) error(ERROR_DEFINE_MACRO_ALREADY_EXISTS, _code[1] + "\nfirst defined here: " + macro_list[i].details_first_defined);
        define_t _define(_code[1], _value, current_file->filename + ", Line " + to_string(current_file->current_line));
        define_list.push_back(_define);
    }
    else error(ERROR_INVALID_EXPRESSION);
    return "";
}

string asm8::directive_macro(vector<string> _code){
    if((_code.size() > 1) && (_code[1].size() > 0)){
        for(uint32_t i = 0; i < define_list.size(); i++)
            if(_code[1] == define_list[i].name) error(ERROR_DEFINE_MACRO_ALREADY_EXISTS, _code[1] + "\nfirst defined here: " + define_list[i].details_first_defined);
        for(uint32_t i = 0; i < macro_list.size(); i++)
            if(_code[1] == macro_list[i].name) error(ERROR_DEFINE_MACRO_ALREADY_EXISTS, _code[1] + "\nfirst defined here: " + macro_list[i].details_first_defined);
        vector<string> _arg_list;
        for(uint32_t i = 2; i < _code.size(); i++){
            if((_code[i] != ",") && (_code[i].length() > 0)) _arg_list.push_back(_code[i]);
        }
        
        uint32_t _macro_start_line = current_file->current_line;
        vector<vector<string>> _macro_code;
        vector<string> _code_line;
        while(!is_directive(_code_line, endm_d)){
            _code_line = split_current_line_into_words();
            current_file->current_line++;
            if(!is_directive(_code_line, endm_d)){
                if(_code_line.size() > 0){
                    _macro_code.push_back(_code_line);
                }
            }
            else{
                break;
            }
            if(current_file->current_line > current_file->total_lines) error(ERROR_ENDM_NOT_FOUND, _code[1] + ", line " + to_string(_macro_start_line));
        }
        
        macro_t _macro(_code[1], _arg_list, _macro_code, current_file->filename + ", Line " + to_string(current_file->current_line));
        macro_list.push_back(_macro);
        
//        //DEBUG
//        cout << "  Macro Name:\t" << _macro.name <<
//                "\n\tArgs:\t"; for(int i = 0; i < _macro.arg_list.size(); i++) cout << _macro.arg_list[i] << "|";
//        cout << "\n\tCode:\t"; for(int i = 0; i < _macro.code.length(); i++)
//                                  if(_macro.code[i] == '\n')    cout << "\n\t\t";
//                                  else                          cout << _macro.code[i];
//        cout << endl;
    }
    else error(ERROR_INVALID_EXPRESSION);
    return "";
}

string asm8::directive_endm(vector<string> _code){
    return "";
}

string asm8::directive_if(vector<string> _code){
    return "";
}

string asm8::directive_endif(vector<string> _code){
    return "";
}

string asm8::directive_dsb(vector<string> _code, uint32_t _addr){
    if(_code.size() < 3) error(ERROR_INVALID_EXPRESSION);
    vector<string> _label;
    _label.push_back(_code[1] + string(":"));
    add_label(_label);
    set_label_addr(get_label_id(_label), _addr);
    return "";
}

void asm8::skip_directive_macro(vector<string> _code){
    uint32_t _macro_start_line = current_file->current_line;
    vector<string> _code_line = split_current_line_into_words();
    while(!is_directive(_code_line, endm_d)){
        _code_line = split_current_line_into_words();
        current_file->current_line++;
        if(is_directive(_code_line, endm_d)) break;
        if(current_file->current_line > current_file->total_lines) error(ERROR_ENDM_NOT_FOUND, _code[1] + ", line " + to_string(_macro_start_line));
    }
}

void asm8::skip_directive_if(vector<string> _code){
    
}

uint32_t asm8::get_db_size(vector<string> _code){
    if(_code.size() < 2) return 0;
    uint32_t _count = 0;
    for(uint32_t i = 1; i < _code.size(); i++){
        if(_code[i][0] == '\"'){
            for(uint32_t j = 1; j < _code[i].size(); j++){
                if(_code[i][j] == '\"') break;
                else                    _count++;
            }
        }
        else if(_code[i] != ",") _count++;
    }
    
    return _count;
}

uint32_t asm8::get_dsb_size(vector<string> _code){
    if(_code.size() < 3) return 0;
    return string_to_num(_code[2]);
}

int32_t asm8::get_label_id(vector<string> _code){
    for(uint32_t i = 0; i < label_list.size(); i++){
        for(uint32_t j = 0; j < _code.size(); j++){
            if(_code[j].substr(0, _code[j].find(":")) == label_list[i].name) return i;
        }
    }
    return -1;
}

void asm8::set_label_addr(uint32_t _label_id, uint32_t _addr){
    label_list[_label_id].address = _addr;
}

int asm8::get_define_value(vector<string> _code){
    for(uint32_t i = 0; i < _code.size(); i++){
        for(uint32_t j = 0; j < define_list.size(); j++){
            if(_code[i] == define_list[j].name) return define_list[j].value;
        }
    }
    return 0;
}

string asm8::get_macro(vector<string> _code){
    return "";
}

asm8::instr_type asm8::get_instr_type(vector<string> _code){
    if(_code.size() < 1)            return empty_instr;
    if(_code[0].size() < 1)         return empty_instr;
    if(is_directive(_code, any_d))  return empty_instr;
    if(is_label(_code))             return empty_instr;
    
    if(_code[0].find("ad") == 0)    return add_instr;
    if(_code[0].find("ac") == 0)    return adc_instr;
    if(_code[0].find("su") == 0)    return sub_instr;
    if(_code[0].find("sb") == 0)    return sbc_instr;
    if(_code[0].find("nd") == 0)    return and_instr;
    if(_code[0].find("xr") == 0)    return xor_instr;
    if(_code[0].find("or") == 0)    return or_instr;
    if(_code[0].find("cp") == 0)    return cp_instr;
    if(_code[0].find("rlc") == 0)   return rlc_instr;
    if(_code[0].find("ral") == 0)   return ral_instr;
    if(_code[0].find("rrc") == 0)   return rrc_instr;
    if(_code[0].find("rar") == 0)   return rar_instr;
    
    if(_code[0].find("halt") == 0)  return halt_instr;
    if(_code[0].find("nop") == 0)   return xor_instr;
    if(_code[0].find("l") == 0)     return ld_instr;
    
    if(_code[0].find("rst") == 0)   return rst_instr;
    if(_code[0].find("r") == 0)     return ret_instr;
    if(_code[0].find("c") == 0)     return cal_instr;
    if(_code[0].find("j") == 0)     return jmp_instr;
    
    if(_code[0].find("inp") == 0)   return inp_instr;
    if(_code[0].find("out") == 0)   return out_instr;
    if(_code[0].find("in") == 0)    return inc_instr;
    if(_code[0].find("de") == 0)    return dec_instr;
    
    return invalid_instr;
}

bool asm8::check_instr_valid(vector<string> _code){
    switch(get_instr_type(_code)){
        case add_instr:
        case adc_instr:
        case sub_instr:
        case sbc_instr:
        case and_instr:
        case xor_instr:
        case or_instr:
        case cp_instr:
            return (((_code[0].substr(2, 1).find_first_of("abcdehlm") != string::npos) && (_code.size() == 1)) ||
                    ((_code[0].substr(2, 1) == "i") && (_code.size() > 1)));
        case halt_instr:
            return ((_code[0] == "halt") && (_code.size() == 1));
        case nop_instr:
            return ((_code[0] == "nop") && (_code.size() == 1));
        case ld_instr:
            return (((_code[0].substr(1, 1).find_first_of("abcdehlm") != string::npos) && (_code[0].substr(2, 1).find_first_of("abcdehlm") != string::npos) && (_code[0] != "lmm") && (_code.size() == 1)) ||
                    ((_code[0].substr(1, 1).find_first_of("abcdehlm") != string::npos) && (_code[0].substr(2, 1) == "i") && (_code.size() > 1)));
        case rlc_instr:
            return ((_code[0] == "rlc") && (_code.size() == 1));
        case ral_instr:
            return ((_code[0] == "ral") && (_code.size() == 1));
        case rrc_instr:
            return ((_code[0] == "rrc") && (_code.size() == 1));
        case rar_instr:
            return ((_code[0] == "rar") && (_code.size() == 1));
        case ret_instr:
            return (((_code[0] == "ret") && (_code.size() == 1)) ||
                    ((_code[0].substr(1, 1).find_first_of("ft") != string::npos) && (_code[0].substr(2, 1).find_first_of("czsp") != string::npos) && (_code.size() == 1)));
        case rst_instr:
            return ((_code[0] == "rst") && (_code.size() > 1));
        case jmp_instr:
            return (((_code[0] == "jmp") && (_code.size() > 1)) ||
                    ((_code[0].substr(1, 1).find_first_of("ft") != string::npos) && (_code[0].substr(2, 1).find_first_of("czsp") != string::npos) && (_code.size() > 1)));
        case cal_instr:
            return (((_code[0] == "cal") && (_code.size() > 1)) ||
                    ((_code[0].substr(1, 1).find_first_of("ft") != string::npos) && (_code[0].substr(2, 1).find_first_of("czsp") != string::npos) && (_code.size() > 1)));
        case inp_instr:
            return ((_code[0] == "inp") && (_code.size() > 1));
        case out_instr:
            return ((_code[0] == "out") && (_code.size() > 1));
        case inc_instr:
            return ((_code[0].substr(2, 1).find_first_of("bcdehl") != string::npos) && (_code.size() == 1));
        case dec_instr:
            return ((_code[0].substr(2, 1).find_first_of("bcdehl") != string::npos) && (_code.size() == 1));
        case empty_instr:
            return true;
        case invalid_instr:
            return false;
        default:
            return false;
    }
}

uint8_t asm8::get_instr_size(vector<string> _code){
    switch(get_instr_type(_code)){
        case add_instr:
        case adc_instr:
        case sub_instr:
        case sbc_instr:
        case and_instr:
        case xor_instr:
        case or_instr:
        case cp_instr:
           if(_code[0].substr(2, 1) == "i") return 2;
           else                             return 1;
        case halt_instr:
            return 1;
        case nop_instr:
            return 1;
        case ld_instr:
            if(_code[0].substr(2, 1) == "i") return 2;
            else                             return 1;
        case rlc_instr:
        case ral_instr:
        case rrc_instr:
        case rar_instr:
            return 1;
        case ret_instr:
            return 1;
        case rst_instr:
            return 1;
        case jmp_instr:
            return 3;
        case cal_instr:
            return 3;
        case inp_instr:
            return 1;
        case out_instr:
            return 1;
        case inc_instr:
            return 1;
        case dec_instr:
            return 1;
        case empty_instr:
            return 0;
        case invalid_instr:
            return 0;
        default:
            return 0;
    }
}

vector<uint8_t> asm8::translate_instr(vector<string> _code){
    vector<uint8_t> _translated_code;
    uint8_t _byte;
    uint16_t _word;
    
    if(is_directive(_code, any_d)){
        if(is_directive(_code, org_d)){
            uint32_t _addr = address;
            uint32_t _org;
            _org = string_to_num(_code[1]);
            while(_addr < _org){
                _translated_code.push_back(0xFF);
                _addr++;
            }
        }
        else if(is_directive(_code, db_d)){
            for(uint32_t i = 1; i < _code.size(); i++){
                if(_code[i][0] == '\"'){
                    for(uint32_t j = 1; (j < _code[i].size()); j++){
                        if((_code[i][j] == '\"')) break;
                        _translated_code.push_back(string_to_num(string("\'") + _code[i][j] + string("\'")));
                    }
                }
                else if(isdigit(_code[i][0])){
                    _translated_code.push_back(string_to_num(_code[i]));
                }
            }
        }
        
        return _translated_code;
    }
    if(is_label(_code)) return _translated_code;
    
    switch(get_instr_type(_code)){
        case add_instr:
            _byte = 0x80;
            if(_code[0].substr(2, 1) == "a")        _byte |= 0x00;
            else if(_code[0].substr(2, 1) == "b")   _byte |= 0x01;
            else if(_code[0].substr(2, 1) == "c")   _byte |= 0x02;
            else if(_code[0].substr(2, 1) == "d")   _byte |= 0x03;
            else if(_code[0].substr(2, 1) == "e")   _byte |= 0x04;
            else if(_code[0].substr(2, 1) == "h")   _byte |= 0x05;
            else if(_code[0].substr(2, 1) == "l")   _byte |= 0x06;
            else if(_code[0].substr(2, 1) == "m")   _byte |= 0x07;
            else if(_code[0].substr(2, 1) == "i"){
                _translated_code.push_back(0x04);
                _byte = string_to_num(_code[1]);
            }
            _translated_code.push_back(_byte);
            break;
        case adc_instr:
            _byte = 0x88;
            if(_code[0].substr(2, 1) == "a")        _byte |= 0x00;
            else if(_code[0].substr(2, 1) == "b")   _byte |= 0x01;
            else if(_code[0].substr(2, 1) == "c")   _byte |= 0x02;
            else if(_code[0].substr(2, 1) == "d")   _byte |= 0x03;
            else if(_code[0].substr(2, 1) == "e")   _byte |= 0x04;
            else if(_code[0].substr(2, 1) == "h")   _byte |= 0x05;
            else if(_code[0].substr(2, 1) == "l")   _byte |= 0x06;
            else if(_code[0].substr(2, 1) == "m")   _byte |= 0x07;
            else if(_code[0].substr(2, 1) == "i"){
                _translated_code.push_back(0x0C);
                _byte = string_to_num(_code[1]);
            }
            _translated_code.push_back(_byte);
            break;
        case sub_instr:
            _byte = 0x90;
            if(_code[0].substr(2, 1) == "a")        _byte |= 0x00;
            else if(_code[0].substr(2, 1) == "b")   _byte |= 0x01;
            else if(_code[0].substr(2, 1) == "c")   _byte |= 0x02;
            else if(_code[0].substr(2, 1) == "d")   _byte |= 0x03;
            else if(_code[0].substr(2, 1) == "e")   _byte |= 0x04;
            else if(_code[0].substr(2, 1) == "h")   _byte |= 0x05;
            else if(_code[0].substr(2, 1) == "l")   _byte |= 0x06;
            else if(_code[0].substr(2, 1) == "m")   _byte |= 0x07;
            else if(_code[0].substr(2, 1) == "i"){
                _translated_code.push_back(0x14);
                _byte = string_to_num(_code[1]);
            }
            _translated_code.push_back(_byte);
            break;
        case sbc_instr:
            _byte = 0x98;
            if(_code[0].substr(2, 1) == "a")        _byte |= 0x00;
            else if(_code[0].substr(2, 1) == "b")   _byte |= 0x01;
            else if(_code[0].substr(2, 1) == "c")   _byte |= 0x02;
            else if(_code[0].substr(2, 1) == "d")   _byte |= 0x03;
            else if(_code[0].substr(2, 1) == "e")   _byte |= 0x04;
            else if(_code[0].substr(2, 1) == "h")   _byte |= 0x05;
            else if(_code[0].substr(2, 1) == "l")   _byte |= 0x06;
            else if(_code[0].substr(2, 1) == "m")   _byte |= 0x07;
            else if(_code[0].substr(2, 1) == "i"){
                _translated_code.push_back(0x1C);
                _byte = string_to_num(_code[1]);
            }
            _translated_code.push_back(_byte);
            break;
        case and_instr:
            _byte = 0xA0;
            if(_code[0].substr(2, 1) == "a")        _byte |= 0x00;
            else if(_code[0].substr(2, 1) == "b")   _byte |= 0x01;
            else if(_code[0].substr(2, 1) == "c")   _byte |= 0x02;
            else if(_code[0].substr(2, 1) == "d")   _byte |= 0x03;
            else if(_code[0].substr(2, 1) == "e")   _byte |= 0x04;
            else if(_code[0].substr(2, 1) == "h")   _byte |= 0x05;
            else if(_code[0].substr(2, 1) == "l")   _byte |= 0x06;
            else if(_code[0].substr(2, 1) == "m")   _byte |= 0x07;
            else if(_code[0].substr(2, 1) == "i"){
                _translated_code.push_back(0x24);
                _byte = string_to_num(_code[1]);
            }
            _translated_code.push_back(_byte);
            break;
        case xor_instr:
            _byte = 0xA8;
            if(_code[0].substr(2, 1) == "a")        _byte |= 0x00;
            else if(_code[0].substr(2, 1) == "b")   _byte |= 0x01;
            else if(_code[0].substr(2, 1) == "c")   _byte |= 0x02;
            else if(_code[0].substr(2, 1) == "d")   _byte |= 0x03;
            else if(_code[0].substr(2, 1) == "e")   _byte |= 0x04;
            else if(_code[0].substr(2, 1) == "h")   _byte |= 0x05;
            else if(_code[0].substr(2, 1) == "l")   _byte |= 0x06;
            else if(_code[0].substr(2, 1) == "m")   _byte |= 0x07;
            else if(_code[0].substr(2, 1) == "i"){
                _translated_code.push_back(0x2C);
                _byte = string_to_num(_code[1]);
            }
            _translated_code.push_back(_byte);
            break;
        case or_instr:
            _byte = 0xB0;
            if(_code[0].substr(2, 1) == "a")        _byte |= 0x00;
            else if(_code[0].substr(2, 1) == "b")   _byte |= 0x01;
            else if(_code[0].substr(2, 1) == "c")   _byte |= 0x02;
            else if(_code[0].substr(2, 1) == "d")   _byte |= 0x03;
            else if(_code[0].substr(2, 1) == "e")   _byte |= 0x04;
            else if(_code[0].substr(2, 1) == "h")   _byte |= 0x05;
            else if(_code[0].substr(2, 1) == "l")   _byte |= 0x06;
            else if(_code[0].substr(2, 1) == "m")   _byte |= 0x07;
            else if(_code[0].substr(2, 1) == "i"){
                _translated_code.push_back(0x34);
                _byte = string_to_num(_code[1]);
            }
            _translated_code.push_back(_byte);
            break;
        case cp_instr:
            _byte = 0xB8;
            if(_code[0].substr(2, 1) == "a")        _byte |= 0x00;
            else if(_code[0].substr(2, 1) == "b")   _byte |= 0x01;
            else if(_code[0].substr(2, 1) == "c")   _byte |= 0x02;
            else if(_code[0].substr(2, 1) == "d")   _byte |= 0x03;
            else if(_code[0].substr(2, 1) == "e")   _byte |= 0x04;
            else if(_code[0].substr(2, 1) == "h")   _byte |= 0x05;
            else if(_code[0].substr(2, 1) == "l")   _byte |= 0x06;
            else if(_code[0].substr(2, 1) == "m")   _byte |= 0x07;
            else if(_code[0].substr(2, 1) == "i"){
                _translated_code.push_back(0x3C);
                _byte = string_to_num(_code[1]);
            }
            _translated_code.push_back(_byte);
            break;
        case halt_instr:
            _translated_code.push_back(0xFF);
            break;
        case nop_instr:
            _translated_code.push_back(0xC0);
        case ld_instr:
            if(_code[0].substr(2, 1) != "i"){
                _byte = 0xC0;
                if(_code[0].substr(1, 1) == "a")        _byte |= 0x00;
                else if(_code[0].substr(1, 1) == "b")   _byte |= 0x08;
                else if(_code[0].substr(1, 1) == "c")   _byte |= 0x10;
                else if(_code[0].substr(1, 1) == "d")   _byte |= 0x18;
                else if(_code[0].substr(1, 1) == "e")   _byte |= 0x20;
                else if(_code[0].substr(1, 1) == "h")   _byte |= 0x28;
                else if(_code[0].substr(1, 1) == "l")   _byte |= 0x30;
                else if(_code[0].substr(1, 1) == "m")   _byte |= 0x38;
                if(_code[0].substr(2, 1) == "a")        _byte |= 0x00;
                else if(_code[0].substr(2, 1) == "b")   _byte |= 0x01;
                else if(_code[0].substr(2, 1) == "c")   _byte |= 0x02;
                else if(_code[0].substr(2, 1) == "d")   _byte |= 0x03;
                else if(_code[0].substr(2, 1) == "e")   _byte |= 0x04;
                else if(_code[0].substr(2, 1) == "h")   _byte |= 0x05;
                else if(_code[0].substr(2, 1) == "l")   _byte |= 0x06;
                else if(_code[0].substr(2, 1) == "m")   _byte |= 0x07;
            }
            else{
                if(_code[0].substr(1, 1) == "a")        _byte = 0x06;
                else if(_code[0].substr(1, 1) == "b")   _byte = 0x0E;
                else if(_code[0].substr(1, 1) == "c")   _byte = 0x16;
                else if(_code[0].substr(1, 1) == "d")   _byte = 0x1E;
                else if(_code[0].substr(1, 1) == "e")   _byte = 0x26;
                else if(_code[0].substr(1, 1) == "h")   _byte = 0x2E;
                else if(_code[0].substr(1, 1) == "l")   _byte = 0x36;
                else if(_code[0].substr(1, 1) == "m")   _byte = 0x3E;
                _translated_code.push_back(_byte);
                _byte = string_to_num(_code[1]);
            }
            _translated_code.push_back(_byte);
            break;
        case rlc_instr:
            _translated_code.push_back(0x02);
            break;
        case ral_instr:
            _translated_code.push_back(0x12);
            break;
        case rrc_instr:
            _translated_code.push_back(0x0A);
            break;
        case rar_instr:
            _translated_code.push_back(0x1A);
            break;
        case ret_instr:
            if(_code[0] == "ret"){
                _translated_code.push_back(0x07);
            }
            else{
                if(_code[0].substr(2, 1) == "c")        _byte = 0x03;
                else if(_code[0].substr(2, 1) == "z")   _byte = 0x0B;
                else if(_code[0].substr(2, 1) == "s")   _byte = 0x13;
                else if(_code[0].substr(2, 1) == "p")   _byte = 0x1B;
                if(_code[0].substr(1, 1) == "t") _byte += 0x20;
                _translated_code.push_back(_byte);
            }
            _word = string_to_num(_code[1]);
            _translated_code.push_back(_word & 0xFF);
            _translated_code.push_back((_word >> 8) & 0xFF);
            break;
        case rst_instr:
            _byte = string_to_num(_code[1]);
            if(_byte > 7) error(ERROR_INVALID_INSTR);
            _byte = 0x05 + (_byte << 3);
            _translated_code.push_back(_byte);
            break;
        case jmp_instr:
            if(_code[0] == "jmp"){
                _translated_code.push_back(0x44);
            }
            else{
                if(_code[0].substr(2, 1) == "c")        _byte = 0x40;
                else if(_code[0].substr(2, 1) == "z")   _byte = 0x48;
                else if(_code[0].substr(2, 1) == "s")   _byte = 0x50;
                else if(_code[0].substr(2, 1) == "p")   _byte = 0x58;
                if(_code[0].substr(1, 1) == "t") _byte += 0x20;
                _translated_code.push_back(_byte);
            }
            _word = string_to_num(_code[1]);
            _translated_code.push_back(_word & 0xFF);
            _translated_code.push_back((_word >> 8) & 0xFF);
            break;
        case cal_instr:
            if(_code[0] == "cal"){
                _translated_code.push_back(0x46);
            }
            else{
                if(_code[0].substr(2, 1) == "c")        _byte = 0x42;
                else if(_code[0].substr(2, 1) == "z")   _byte = 0x4A;
                else if(_code[0].substr(2, 1) == "s")   _byte = 0x52;
                else if(_code[0].substr(2, 1) == "p")   _byte = 0x5A;
                if(_code[0].substr(1, 1) == "t") _byte += 0x20;
                _translated_code.push_back(_byte);
            }
            _word = string_to_num(_code[1]);
            _translated_code.push_back(_word & 0xFF);
            _translated_code.push_back((_word >> 8) & 0xFF);
            break;
        case inp_instr:
            _byte = string_to_num(_code[1]);
            if(_byte > 7) error(ERROR_INVALID_INSTR);
            _byte = 0x41 + (_byte << 1);
            _translated_code.push_back(_byte);
            break;
        case out_instr:
            _byte = string_to_num(_code[1]);
            if((_byte < 8) || (_byte > 31)) error(ERROR_INVALID_INSTR);
            _byte = 0x41 + (_byte << 1);
            _translated_code.push_back(_byte);
            break;
        case inc_instr:
            if(_code[0].substr(2, 1) == "b")        _byte = 0x08;
            else if(_code[0].substr(2, 1) == "c")   _byte = 0x10;
            else if(_code[0].substr(2, 1) == "d")   _byte = 0x18;
            else if(_code[0].substr(2, 1) == "e")   _byte = 0x20;
            else if(_code[0].substr(2, 1) == "h")   _byte = 0x28;
            else if(_code[0].substr(2, 1) == "l")   _byte = 0x30;
            _translated_code.push_back(_byte);
            break;
        case dec_instr:
            if(_code[0].substr(2, 1) == "b")        _byte = 0x09;
            else if(_code[0].substr(2, 1) == "c")   _byte = 0x11;
            else if(_code[0].substr(2, 1) == "d")   _byte = 0x19;
            else if(_code[0].substr(2, 1) == "e")   _byte = 0x21;
            else if(_code[0].substr(2, 1) == "h")   _byte = 0x29;
            else if(_code[0].substr(2, 1) == "l")   _byte = 0x31;
            _translated_code.push_back(_byte);
            break;
        case empty_instr:
            break;
        case invalid_instr:
            error(ERROR_INVALID_INSTR);
        default:
            break;
    }
    return _translated_code;
}

int asm8::error(int _error_code, string _str){
    string _error;
    switch(_error_code){
        case ERROR_INVALID_EXPRESSION:
            _error = "Invalid Expression"; break;
        case ERROR_OPEN_FILE:
            _error = "Could not open file: " + _str; break;
        case ERROR_CREATE_FILE:
            _error = "Could not create/overwrite file: " + _str; break;
        case ERROR_FILE_STACK_OVERFLOW:
            _error = "File Stack overflow; Too many files opened"; break;
        case ERROR_DEFINE_MACRO_ALREADY_EXISTS:
            _error = "Define/Macro already exists: " + _str; break;
        case ERROR_INVALID_LABEL:
            _error = "Label is invalid"; break;
        case ERROR_LABEL_ALREADY_EXISTS:
            _error = "Label already exists: " + _str; break;
        case ERROR_ENDM_NOT_FOUND:
            _error = "Fitting .endm directive not found for Macro: " + _str; break;
        case ERROR_MISSING_MACRO_ARGS:
            _error = "Missing Macro Arguments"; break;
        case ERROR_MAX_TRIES_PASS_2:
            _error = "Required too many tries on Pass 2"; break;
        case ERROR_INVALID_INSTR:
            _error = "Invalid Instruction"; break;
        case ERROR_ORG_BACK:
            _error = ".org smaller than current PC"; break;
    }
    cerr << ((asm8::asm8_p->current_file != nullptr) ? string(asm8::asm8_p->current_file->filename + ", ") : string("")) <<
            ((asm8::asm8_p->current_file != nullptr) ? string("Line ") + to_string(asm8::asm8_p->current_file->current_line) + string(":\n") : string("")) <<
            "Error: " << _error << endl;
    exit(-1);
}



