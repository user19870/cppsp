#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <string.h>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <functional>
bool isWindows=false;bool isMac=false;bool isLinux =false;
 #if defined(_WIN32) || defined(_WIN64) 
 #define isWindows 1 
 typedef unsigned int UINT;
typedef int BOOL;

extern "C" BOOL __stdcall SetConsoleOutputCP(UINT wCodePageID);
extern "C" BOOL __stdcall SetConsoleCP(UINT wCodePageID);
 #elif defined(__APPLE__) && defined(__MACH__) 
 #define isMac 1 
 #elif defined(__linux__) 
 #define isLinux 1 
 #endif
//檢查dll依賴:objdump -p cppsp_compiler.exe | findstr ".dll" 
//備忘錄:設計匿名函數lamda(x,=y,&z,type g,{...})，x是變數名無須也不能提前宣告，type ...是參數，=/& ...是傳值，{...}是內容
namespace fs = std::filesystem;
bool Ifiostream=0;bool commentInReg=false;bool shouldInjectFuction=true;
// ====== 新增：萬用語法指令註冊器 ======
std::unordered_map<std::string, std::function<std::string(const std::string&)>> cpsCommands;
std::unordered_map<std::string, std::string>func_head_end ;
void registerCommand(const std::string& name,const std::string& start,const std::string& end,
                     std::function<std::string(const std::string&)> handler) {
    cpsCommands[name] = handler; 
     func_head_end[start]=end;
}
// ======token區域=======

enum class TokenType {
    ROOT,       //0 container 根節點，不算實際 token
    BEGIN,  //1 ( 、 {
    END,     //2 ) 、 }

    IDENTIFIER, //3 變數名稱 
    NUMBER,     //4 數字
    STRING,     //5 字串
    CHAR,       //6 字元
    OPERATOR,   //7 運算子+ - = += * /
    TYPE,       //8 資料型態int float bool string
    SEPARATOR,  //9 分隔符號: , ;

    KEYWORD,    //10 關鍵字: print println input @inject @function
    COMMENT,    //11 註解
    INJECT,     //12 <<...>> 內嵌程式碼
    funcKEYWORD,    //13 函數關鍵字: 
    funcIDENTIFIER,    //14 函數名稱:
    NAMESPACELIKE, //15 C++中使用 n::child呼叫
    STRUCTLIKE, //16 C++中使用 n.child呼叫
    UNKNOWN     //17 未知

};
struct Token {
    TokenType type;
    std::string value;
    size_t line_number;
    std::vector<Token> children; // 用於括號、{}、<<>> 等內部 token
        bool operator==(const Token& other) const {
        return type == other.type && value == other.value;
    }
};
struct TokenizeState {  
    int depth = 0;
    bool inString = false;
    bool inBlockComment = false;
};
TokenizeState state={0,false,false};
std::unordered_set<std::string>  typeKeywords= {
        "int", "float", "bool", "char", "string","struct"
    };
bool isTypeKeyword(const std::string& s) {
    return typeKeywords.find(s) != typeKeywords.end();
}
 
static const std::vector<std::string> operators = {
    ">>=", "<<=",
    "==", "!=", "<=", ">=",
    "&&", "||",
    "<<", ">>",
    "+=", "-=", "*=", "/=", "%=",
    "&=", "|=", "^=",
    "++", "--",
    "=", "+", "-", "*", "/", "%",
    "&", "|", "^", "!", "<", ">","::",":"
};

std::vector<Token> tokenstream;

using tokencallback = std::function<std::string(const Token&)>;
std::unordered_map<std::string, tokencallback> tokenHandlers;
void registerToken(const std::string& name, tokencallback handler) {
    tokenHandlers[name] = handler;
}

std::unordered_map<std::string, tokencallback> funcHandlers;std::unordered_set<std::string> what_is_funcname;
std::unordered_set<std::string> what_is_namespace_like;
std::unordered_set<std::string> what_is_struct_like;
std::unordered_set<std::string> custom_command;std::unordered_map<std::string,std::vector<Token>> custom_command_arg;
void registerfunc(const std::string& name, tokencallback handler) {
    funcHandlers[name] = handler;
}

Token tokenizeFile(std::istream& funcfile,size_t lineno = 1) {
   // 一次把整個檔案讀進來（不用 get / peek）
    std::string src(
        (std::istreambuf_iterator<char>(funcfile)),
        std::istreambuf_iterator<char>()
    );

    Token root{TokenType::ROOT, "", 0, {}};

    size_t i = 0;
     
    TokenizeState state{};
    

    auto skipWhitespace = [&](size_t& idx) {
        while (idx < src.size()) {
            if (src[idx] == ' ' || src[idx] == '\t') {
                idx++;
            } else if (src[idx] == '\n') {  root.children.push_back({TokenType::SEPARATOR,";",lineno,{}});
                lineno++;
                idx++;
            } else {
                break;
            }
        }
         
         
    };

    auto parseIdentifierOrKeyword = [&](size_t& idx) -> Token {
        std::string id;
        while (idx < src.size() &&
              ((src[idx] >= 'a' && src[idx] <= 'z') ||
               (src[idx] >= 'A' && src[idx] <= 'Z') ||
               (src[idx] >= '0' && src[idx] <= '9') ||
               src[idx] == '_' || src[idx] == '@' || src[idx] == '#'||src[idx]==':'||src[i]=='.')) {
                if(src[idx]==':'&&src[idx+1]==':'){id+="::";idx+=2;continue;}
                if(src[idx]=='.'){idx++;break;;}
            id += src[idx++];
        }

        TokenType ttype = TokenType::IDENTIFIER;
        if (isTypeKeyword(id))
           ttype = TokenType::TYPE;
        if (tokenHandlers.find(id) != tokenHandlers.end())
            ttype = TokenType::KEYWORD;       
         if (funcHandlers.find(id) != funcHandlers.end())
            ttype = TokenType::funcKEYWORD; 
               

        return {ttype, id, lineno, {}};
    };

auto justconnectidentifier = [&](size_t& idx) -> Token {
        std::string id;
        while (idx < src.size() &&
              ((src[idx] >= 'a' && src[idx] <= 'z') ||
               (src[idx] >= 'A' && src[idx] <= 'Z') ||
               (src[idx] >= '0' && src[idx] <= '9') ||
               src[idx] == '_' || src[idx] == '@' || src[idx] == '#'||src[idx]==':'||src[i]=='.')) {
                if(src[idx]==':'&&src[idx+1]==':'){id+="::";idx+=2;continue;}
                 
            id += src[idx++];
        }

        TokenType ttype = TokenType::IDENTIFIER;
        if (isTypeKeyword(id))
           ttype = TokenType::TYPE;
        if (tokenHandlers.find(id) != tokenHandlers.end())
            ttype = TokenType::KEYWORD;       
         if (funcHandlers.find(id) != funcHandlers.end())
            ttype = TokenType::funcKEYWORD; 
               

        return {ttype, id, lineno, {}};
    };

    auto parseNumber = [&](size_t& idx) -> Token {
        std::string num;
        while (idx < src.size() &&
              ((src[idx] >= '0' && src[idx] <= '9') || src[idx] == '.')) {
            num += src[idx++];
        }
        return {TokenType::NUMBER, num, lineno, {}};
    };

    while (i < src.size()) {
        skipWhitespace(i);
        if (i >= src.size()) break;

        //單行判斷的關鍵字類似import a,b,c...
        std::vector<std::string> regkw={"import","from ","use","package"};
        for(auto regk:regkw){
            if ( i + 1 < src.size()&&src.compare(i,regk.size(),regk)==0) {
            std::string Inimport;Token node{TokenType::funcKEYWORD, regk, lineno, {}}; 
            i+=regk.size();
            while (i < src.size() && src[i] != '\n') {
                Inimport += src[i++];
            }
            node.children.push_back({TokenType::COMMENT, Inimport, lineno, {}});
            root.children.push_back(node);
            continue;
        }
        }
       
         

        // 單行註解
        if ((src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/')
        ||(src[i]=='/'&&i + 1 < src.size() && src[i + 1] == '*') ){
            std::string begin=(src[i+1]=='*')? "/*":"//";
            std::string comment;
            if(begin=="//"){
            while (i < src.size() && src[i] != '\n') {
                comment += src[i++];
            }}else{comment+="/*";i+=2;
                while (i < src.size()) {
                    if(src[i]=='*'&&src[i+1]=='/'){comment+="*/";i+=2;break;}
                comment += src[i++];
            }
            }
            root.children.push_back({TokenType::COMMENT, comment+"\n", lineno, {}});
            continue;
        }

        // 字串
        bool ifwstr =(src[i]=='L'&& i+1<src.size() &&src[i+1] == '"');
        if (ifwstr||(src[i] == '"') ){
            std::string str;
              if(ifwstr){ str="L\"";i+=2;}  else{str="\"";i++; }
            while (i < src.size()) {
                  if(src[i] == '\\'&&src[i+1]=='\"') {  str+='"';i+=2; continue; }
                if (src[i] == '\\' && i + 1 < src.size()) {
                    str += src[i];
                    str += src[i + 1];
                    i += 2;
                    continue;
                }
                   
                
                bool ifstrend;if(src[i+1]==')'||src[i+1]==','||src[i+1]==' '||src[i+1]=='\n') ifstrend=true; else ifstrend=false;
                   
                   if (src[i] == '"'&&ifstrend) {
                    str += '"';
                    i++;
                    break;
                }
            
               
                if (src[i] == '\n') lineno++;
                str += src[i++] ;
            }
            root.children.push_back({TokenType::STRING, str, lineno, {}});
            continue;
        }
        // 字元char
        if(src[i]=='\''){
            std::string charstr;
            charstr+="'";
            i++;
            while(i<src.size()){
                if(src[i]=='\\'&& i+1<src.size()){
                    charstr+=src[i];
                    charstr+=src[i+1];
                    i+=2;
                    continue;
                }
                 bool ifstrend;if(src[i+1]==')'||src[i+1]==',') ifstrend=true; else ifstrend=false;  
                if(src[i]=='\''&&ifstrend){
                    charstr+="'";
                    i++;
                    break;
                }
                if(src[i]=='\n') lineno++;
                charstr+=src[i++];
            }
            root.children.push_back({TokenType::CHAR,charstr,lineno,{}});
            continue;
        }
        

        // () {}
         int inner_lineno = lineno;
        if (src[i] == '(' || src[i] == '{'||src[i]=='[') {
            char startChar = src[i];
            char endChar = (startChar == '(') ? ')' :(startChar == '[') ?']': '}';
            Token node{TokenType::BEGIN, std::string(1, startChar), lineno, {}};

            int depth = 1;
            i++;
            std::string inner;

            while (i < src.size() && depth > 0) {
    
                if (src[i] == startChar){ depth++;}
                if (src[i] == endChar) {depth--;}
                if (depth > 0) {
                    if (src[i] == '\n') lineno++;
                    inner += src[i];
                }
                i++;
            }
 
             
          
             if(startChar =='['&&!root.children.empty()&&!root.children.back().children.empty()&&root.children.back().type == TokenType::NAMESPACELIKE){
                 Token* node = &root; //指標必免值覆蓋
while (!node->children.empty()) {  node = &node->children.back(); }
                if (!inner.empty()){ //把[]變成變數值(和原來變數合併)
             node->value += std::string(1,'<')+inner+std::string(1, '>');
             what_is_struct_like.insert(node->value);
 }}      
            else if(startChar =='['&&!root.children.empty()&&!root.children.back().children.empty()
            &&(root.children.back().value=="function"||root.children.back().value=="var")){
              Token* node = &root; //指標必免值覆蓋
while (!node->children.empty()) {  node = &node->children.back(); }
                if (!inner.empty()){ //把[]變成變數值(和原來變數合併)
               node->value += std::string(1,'<')+inner+std::string(1, '>');
               what_is_struct_like.insert(node->value);
 }} 
            else if (!root.children.empty() && (root.children.back().type == TokenType::KEYWORD
                 ||root.children.back().type==TokenType::funcIDENTIFIER||root.children.back().type==TokenType::funcKEYWORD
                ||root.children.back().type==TokenType::NAMESPACELIKE||root.children.back().type==TokenType::STRUCTLIKE) ){ 
                if (!inner.empty()) {  //變成關鍵字子節點
                
               std::istringstream ss(inner);Token iner=tokenizeFile(ss,inner_lineno);
               for(auto& child : iner.children){  
                node.children.push_back(child); // 不再使用 reinterpret_cast
                if(startChar=='['&&root.children.back().type==TokenType::funcKEYWORD){
                     if(child.value!="]"&&child.value!=",") what_is_funcname.insert(child.value);
                   }
                }
                         }
                  node.children.push_back({TokenType::END, std::string(1, endChar), lineno, {}});
              if(!root.children.empty() && !root.children.back().children.empty()&& custom_command.find(root.children.back().children.back().value)!=custom_command.end()){
            
                if(!root.children.back().children.back().children.empty()){ root.children.back().children.push_back(node); continue;}
                else  {root.children.back().children.back().children.push_back(node);continue;}
              }else{
                   // 把 BEGIN 作為前一個 KEYWORD 的 child
                 root.children.back().children.push_back(node);}  
}else if(startChar =='['&&!root.children.empty() && root.children.back().type == TokenType::IDENTIFIER){
 if (!inner.empty()){ //把[]變成變數值(和原來變數合併)
    root.children.back().value += std::string(1,startChar)+inner+std::string(1, endChar);
 }
}else if(startChar =='['&&!root.children.empty() &&root.children.back().type == TokenType::TYPE){
 if (!inner.empty()){ //把[]變成變數值(和原來變數合併)
    root.children.back().value += std::string(1,'<')+inner+std::string(1, '>');
    what_is_struct_like.insert(root.children.back().value);
 }
}
else   {
               if (!inner.empty()) {  // 單獨()或{}，與關鍵字
                     std::istringstream ss(inner);Token iner=tokenizeFile(ss,inner_lineno);
                    for(auto& child : iner.children) node.children.push_back(child); // 不再使用 reinterpret_cast  
                          }
                         node.children.push_back({TokenType::END, std::string(1, endChar), lineno, {}});
    root.children.push_back(node);
} 
 
            continue;
        }

        // <{ }>
   if (i + 1 < src.size() &&( src[i] == '<' && src[i + 1] == '{')||(src[i]=='<'&&src[i+1]=='<') ){
    std::string startStr,endStr;
    if(src[i]=='<'&&src[i+1]=='<'){
                if(!root.children.empty() && (root.children.back().value=="@function")){  startStr = "<<";
      endStr = ">>";   
    Token node{TokenType::BEGIN, startStr, lineno, {}}; 
    int depth = 1;
    i += 2;
    std::string inner; 
    size_t inner_lineno = lineno;

    while (i + 1 < src.size() && depth > 0) {
        if (src.compare(i, 2, startStr) == 0) {
            inner +="<<";
            i += 2;
            continue;
        }
        if (src.compare(i, 2, endStr) == 0&&(src[i+2]=='\n'||!src[i+2])) {
            depth--;
            i += 3;
            continue;
        }

        if (src[i] == '\n') lineno++;
        inner += src[i++];
    }

    if (!inner.empty()) {
        node.children.push_back({TokenType::INJECT, inner, inner_lineno, {}});
      node.children.push_back({TokenType::END, endStr, lineno, {}});
      root.children.back().children.push_back(node);
       
    } 
    }
      

  }else{
                  startStr = "<{";
                  endStr = "}>";
                  Token node{TokenType::BEGIN, startStr, lineno, {}}; 
    int depth = 1;
    i += 2;
    std::string inner; 
    size_t inner_lineno = lineno;

    while (i + 1 < src.size() && depth > 0) {
        if (src.compare(i, 2, startStr) == 0) {
            depth++;
            i += 2;
            continue;
        }
        if (src.compare(i, 2, endStr) == 0) {
            depth--;
            i += 2;
            continue;
        }

        if (src[i] == '\n') lineno++;
        inner += src[i++];
    }

    if (!inner.empty()) {
      root.children.push_back(node);
      root.children.push_back({TokenType::INJECT, inner, inner_lineno, {}});
      root.children.push_back({TokenType::END, endStr, lineno, {}});
    }
            }
   
 

   
    continue;
}
//var   變數系統
auto var_nodes = [](const Token& node){
Token result =node;
while(!result.children.empty()) result=result.children.back();
return result;
};
       int var_inner_lineno = lineno;
       if (src[i] == 'v' && src[i+1] == 'a' && src[i+2] == 'r' ) {
             
            Token node{TokenType::KEYWORD, "var", lineno, {}};Token invar;
            i += 3;bool varend=true;
  
            std::string inner;
            while (i < src.size()&&varend) {if(!src[i+1]){inner+=src[i];}
                 std::istringstream ss(inner);
                    if (src[i] == '\n'||!src[i+1]){Token tmp=tokenizeFile(ss,var_inner_lineno);
                        while((tmp.children.back().value==";"))tmp.children.pop_back();
                        
                        invar=tmp;
                         
                        for(auto& child : invar.children){  
                   if( child.type != TokenType::TYPE) node.children.push_back(child);
                                        }
                    if(invar.children.back().type==TokenType::TYPE){varend=false;
                    node.children.push_back(invar.children.back());} 
                   
                    
                  
                      else {inner="";}
                      lineno++;
                }
                    inner += src[i];i++;
                    

            }
           /* if(what_is_struct_like.find(invar.children.back().value) != what_is_struct_like.end()){
                for(Token& p:invar.children){
                    if(p.type==TokenType::IDENTIFIER){ p.type==TokenType::STRUCTLIKE;
                    what_is_struct_like.insert(p.value);}
                }
            }*/
            
 
           
    root.children.push_back(node);
  


 
            continue;
        }



        // 運算子 + = > ...
     bool matched = false;
         for (const auto& op : operators) {
         if (i + op.size() <= src.size() &&
         src.compare(i, op.size(), op) == 0) {
  
                if(op=="::"){  i+=op.size(); continue; }
                root.children.push_back({TokenType::OPERATOR, op, lineno, {}} );
             if(!root.children.empty() && (root.children.back().value=="@function")){i+=op.size(); continue; }
        
        i += op.size(); matched = true;
        break;
    }
}

if (matched) continue;

       if(src[i] == ',' || src[i] == ';') {
            root.children.push_back({TokenType::SEPARATOR, std::string(1, src[i]), lineno, {}});
            i++;
            continue;
        }

        // 數字
        if (src[i] >= '0' && src[i] <= '9') {
            root.children.push_back(parseNumber(i));
            continue;
        }

        // 識別字
        if ((src[i] >= 'a' && src[i] <= 'z') ||
            (src[i] >= 'A' && src[i] <= 'Z') ||
            src[i] == '_' || src[i] == '@' || src[i] == '#'||src[i]==':'||src[i]=='.') {int depth=0;
                Token whole=justconnectidentifier(i),node;std::stringstream ss(whole.value);std::string nod;i-=whole.value.size();
                while(getline(ss,nod,'.')){ depth++;
                    

                Token v=parseIdentifierOrKeyword(i);std::string end;bool hasfuncname=false; 
                if(!root.children.empty() &&root.children.back().type == TokenType::funcKEYWORD
            && v.type != TokenType::KEYWORD && v.type != TokenType::funcKEYWORD ){
                
                if(root.children.back().value=="@function"){if (what_is_funcname.find(v.value) != what_is_funcname.end()&&v.type != TokenType::TYPE)   v.type = TokenType::funcIDENTIFIER;
                    root.children.push_back(v);continue;}
                    if(root.children.back().value=="function"){
                        if(whole.value.find(".")!=std::string::npos){
                             if (what_is_namespace_like.find(v.value) != what_is_namespace_like.end()&&v.type != TokenType::TYPE
                ) v.type = TokenType::NAMESPACELIKE;
                
                            Token* cur = &node;//指向node地址
                        if(depth==1){node=v;cur=&node;continue;}
                    if(depth>1){
                        while(!cur->children.empty()){cur = &cur->children.back();} //指向還沒有子節點的a.b.c的最後面一個節點
                        v.value="."+v.value;if(v.type != TokenType::TYPE){ cur->children.push_back(v); //把a.b.c逐漸從a變a.b變a.b.c
                            continue;}else{cur->children.push_back(v);root.children.back().children.push_back(node);continue;}

                    }
                         }
                      if(v.type != TokenType::TYPE) v.type = TokenType::funcIDENTIFIER;
                      what_is_funcname.insert(v.value);
                    }else{
                        
                         if(root.children.back().value=="namespace"){what_is_namespace_like.insert(v.value); if(v.type != TokenType::TYPE) v.type = TokenType::NAMESPACELIKE;}
                       if(root.children.back().value=="struct"){what_is_struct_like.insert(v.value); typeKeywords.insert(v.value); if(v.type != TokenType::TYPE) v.type = TokenType::STRUCTLIKE;}
                       if(root.children.back().value=="@custom"){custom_command.insert(v.value);what_is_funcname.insert(v.value);  v.type = TokenType::funcIDENTIFIER;}
                       
                    }
                   
                      if(!root.children.back().children.empty()&&!root.children.back().children.back().children.empty() 
                      && (root.children.back().children.back().children.back().value== "}"||root.children.back().children.back().children.back().value=="]")&&v.type != TokenType::TYPE) {v.type = TokenType::IDENTIFIER; root.children.push_back(v);continue;} 
                      //上面那段用來避免全域呼叫被丟到function底下 
                      for(auto &child:root.children.back().children){ if (what_is_funcname.find(child.value) != what_is_funcname.end())hasfuncname=true;}
                      if(hasfuncname&&v.type!=TokenType::TYPE&&!(what_is_struct_like.find(root.children.back().value) != what_is_struct_like.end())
                    ){v.type = TokenType::IDENTIFIER;root.children.push_back(v);continue;}
                    root.children.back().children.push_back(v);
                }
                else{
                    if (what_is_funcname.find(v.value) != what_is_funcname.end()&&v.type != TokenType::TYPE
                )  v.type = TokenType::funcIDENTIFIER;
                if (what_is_namespace_like.find(v.value) != what_is_namespace_like.end()&&v.type != TokenType::TYPE
                ) v.type = TokenType::NAMESPACELIKE;
                if (what_is_struct_like.find(v.value) != what_is_struct_like.end()&&v.type != TokenType::TYPE
                ) v.type = TokenType::STRUCTLIKE;
                if(!root.children.empty()&& root.children.back().type == TokenType::TYPE&&what_is_struct_like.find(root.children.back().value) != what_is_struct_like.end()) {
                        what_is_struct_like.insert(v.value);v.type=TokenType::STRUCTLIKE;root.children.push_back(v);continue;}
                if(!root.children.empty()&& root.children.back().type == TokenType::NAMESPACELIKE&&v.value.find(".")==std::string::npos) {
                       what_is_struct_like.insert(v.value);v.type=TokenType::NAMESPACELIKE;root.children.push_back(v);continue;}
                if(!root.children.empty()&&custom_command.find(v.value)!=custom_command.end()&&root.children.back().type!=TokenType::SEPARATOR
            &&root.children.back().value!="var"){
                    v.type = TokenType::funcIDENTIFIER;
                    root.children.back().children.push_back(v);continue;
                }
                

                    if(whole.value==v.value){root.children.push_back(v);} //沒有a.b.c只有一層
                    else{Token* cur = &node;//指向node地址
                        if(depth==1){node=v;cur=&node;}
                    if(depth>1){
                        while(!cur->children.empty()){cur = &cur->children.back();} //指向還沒有子節點的a.b.c的最後面一個節點
                        if(custom_command.find(v.value)!=custom_command.end()){  ;node.value=whole.value;node.type=TokenType::funcIDENTIFIER;node.children={};}
                        else{v.value="."+v.value; cur->children.push_back(v);}//把a.b.c逐漸從a變a.b變a.b.c
                    }
                    
                          }
                }
            }
            if(depth>1&&!root.children.empty()&&root.children.back().value=="function")continue;
            if(depth>1){ root.children.push_back(node); }
            continue;
        }

        if (src[i] == '\n'){lineno++;}
        i++; // 防止死循環
    }

   
    return root;
}



// ====== token 執行接口 ======
// runTokenFunc 對應原 funcfile while，執行 token handler
  Token singletoken(const Token& node, const std::string& afterSeper);
std::string sumLeftparen(const Token& node) {
    std::string rt=node.value,result;
    
    
    for(auto& p:node.children){rt+=sumLeftparen(p);}    
   
    return rt;
}

std::string AdotBdotC(const Token& node,std::string spepar="") {
    std::string result=node.value,callb,sp,cur;
     if(node.type==TokenType::NAMESPACELIKE) sp="::";else if(node.type==TokenType::STRUCTLIKE) sp="."; else sp=spepar;
    if(node.value.find(".")!=std::string::npos){
        if(node.children.empty()){ cur=node.value.substr(node.value.find(".")+1,node.value.size()-1);return spepar+cur; }
        else { cur=node.value.substr(node.value.find(".")+1,node.value.size()-1);cur+=AdotBdotC(node.children[0],sp);return spepar+cur; }
    }
    for(auto& p:node.children){
      if(p.value=="(")  callb+=sumLeftparen(p);
     else result+=AdotBdotC(p,sp);}    
    return result+" "+callb;
}

std::string sumLeftbrackets(const Token& node) {
    std::string result=node.value; 
    if(node.type==TokenType::NAMESPACELIKE||node.type==TokenType::STRUCTLIKE){result= AdotBdotC(node);return result;}
     if(node.type == TokenType::KEYWORD&& !node.children.empty()){ auto it = tokenHandlers.find(node.value);Token k;k.type=TokenType::UNKNOWN;if (it != tokenHandlers.end()){
            k.value=it->second(node);
        }return k.value; }
    if(node.type == TokenType::funcKEYWORD& !node.children.empty()){ auto it = funcHandlers.find(node.value);Token k;k.type=TokenType::UNKNOWN;if (it != funcHandlers.end()){
           k.value=it->second(node);
        }return k.value; }

if(node.type==TokenType::funcIDENTIFIER&&!node.children.empty()){

         auto cusit =custom_command_arg.find(node.value);
        if(cusit!=custom_command_arg.end()){  
            std::vector<Token> templa=cusit->second;std::vector<size_t> args;Token out;bool hasback=false;std::string tmp;
            for(size_t pos=0;pos<templa.size();pos++){if(templa[pos].type==TokenType::INJECT)args.push_back(pos);}
            for(size_t i=0,j=0;i<node.children[0].children.size();i++){ Token cur=node.children[0].children[i];cur.value=singletoken(cur,",").value;
                if(cur.value!="<{"&&cur.value!="}>"){
                     if(cur.value=="{"){ for(auto& inside:cur.children)cur.value+=singletoken(inside,";").value;}
                    if(cur.value==","||cur.value==")"){j++;tmp="";continue;}else{tmp+=" "+cur.value;}
                    if(j>=args.size()){j=0;hasback=true; }
                     
                   if(!hasback){templa[args[j]].value=tmp;} else{templa[args[j]].value+=tmp;}
                   
                }
            }
           for(auto& p:templa){out.value+=p.value;}
           out.type=TokenType::INJECT;
         return out.value;
        }
                 
            }
        
    for(auto& p:node.children){
         
         result+=sumLeftbrackets(p);}    
   
    return result;
}

Token mergetoken(const Token& node, const std::string& afterSeper) {

      if(node.type == TokenType::STRING){return node;} 
      if(node.type == TokenType::NUMBER){return node;}
       if(node.type == TokenType::IDENTIFIER&&node.children.empty()){ return node;}
        if(node.type == TokenType::IDENTIFIER&& !node.children.empty()){   }
        if (node.type == TokenType::KEYWORD&&(node.value=="true"||node.value=="false")) {Token tfbool;tfbool.type==TokenType::STRING;tfbool.value="\""+node.value+"\""; return tfbool;}
 if (node.type == TokenType::SEPARATOR) {Token aftp;aftp.type==TokenType::SEPARATOR;aftp.value=afterSeper; return  aftp;}
  Token result; result.type = TokenType::STRING;
    for (const auto& child : node.children) {
        result.value += mergetoken(child,afterSeper).value;
    }
    

    return result; }
    Token singletoken(const Token& node, const std::string& afterSeper) {
        Token result; bool boolkey =(node.value=="true"||node.value=="false")?true:false;

      if(node.type == TokenType::STRING){result.type = TokenType::STRING;return node;} 
      if(node.type == TokenType::CHAR){result.type = TokenType::CHAR;return node;} 
      if(node.type == TokenType::NUMBER){result.type = TokenType::NUMBER; return node;}
      if(node.type == TokenType::OPERATOR){result.type = TokenType::NUMBER; return node;}
      if(node.type == TokenType::INJECT){result.type = TokenType::INJECT; return node;}
      if(node.value=="<{"||node.value=="}>" ){return result;}
      if(node.value=="{"){result.value+=sumLeftbrackets(node);return result;}
      if(node.type==TokenType::funcIDENTIFIER&&!node.children.empty()){

         auto cusit =custom_command_arg.find(node.value);
        if(cusit!=custom_command_arg.end()){  
            std::vector<Token> templa=cusit->second;std::vector<size_t> args;Token out;bool hasback=false;std::string tmp;
            for(size_t pos=0;pos<templa.size();pos++){if(templa[pos].type==TokenType::INJECT)args.push_back(pos);}
            for(size_t i=0,j=0;i<node.children[0].children.size();i++){ Token cur=node.children[0].children[i];cur.value=singletoken(cur,",").value;
                if(cur.value!="<{"&&cur.value!="}>"){
                     if(cur.value=="{"){ for(auto& inside:cur.children)cur.value+=singletoken(inside,";").value;}
                    if(cur.value==","||cur.value==")"){j++;tmp="";continue;}else{tmp+=" "+cur.value;}
                    if(j>=args.size()){j=0;hasback=true; }
                     
                   if(!hasback){templa[args[j]].value=tmp;} else{templa[args[j]].value+=tmp;}
                   
                }
            }
           for(auto& p:templa){out.value+=p.value;}
           out.type=TokenType::INJECT;
         return out;
        }
                 bool iftemplate=false;  for(auto& child:node.children){  if(child.value!="(")iftemplate=true;}
             if(!iftemplate)result.value=node.value+"(" ;else  result.value=node.value; 
                for(auto& child:node.children){
                    if(child.value!="("){result.value+=singletoken(child,",").value+"(";continue;}
                    for(auto& p:child.children){
                    if(p.value=="("){result.value+=sumLeftparen(p);continue;}
                    result.value+=singletoken(p,",").value;
                }
                }
                return result;
            }
       if(node.type == TokenType::TYPE){
           
          if(node.value=="float"){result.value="double";result.type = TokenType::TYPE;return result;}
          if(node.value=="string"){result.value="std::string";result.type = TokenType::TYPE;return result;}
          if(what_is_struct_like.find(node.value)!=what_is_struct_like.end()){result=node;result.value+=" ";return result;}
       }
      
    if(node.type == TokenType::IDENTIFIER&&node.children.empty()){
        if(node.value=="true"||node.value=="false"){Token tfbool;tfbool.type=TokenType::STRING;
            result.type = TokenType::KEYWORD;//tfbool.value="\""+node.value+"\"";
           tfbool.value=node.value;  return tfbool;}else{
        result.type = TokenType::IDENTIFIER; return node;}  }
    if(node.type==TokenType::NAMESPACELIKE||node.type==TokenType::STRUCTLIKE){Token n=node;n.value=AdotBdotC(node);return n;}
    if(node.type == TokenType::KEYWORD ){ auto it = tokenHandlers.find(node.value);Token k;k.type=TokenType::UNKNOWN;if (it != tokenHandlers.end()){
            k.value=it->second(node);
        }return k; }
    if(node.type == TokenType::funcKEYWORD ){ auto it = funcHandlers.find(node.value);Token k;k.type=TokenType::UNKNOWN;if (it != funcHandlers.end()){
           k.value=it->second(node);
        }return k; }
    if (node.type == TokenType::SEPARATOR) {Token aftp;aftp.type=TokenType::SEPARATOR;result.type = TokenType::SEPARATOR;aftp.value=afterSeper; return  aftp;}
    return node; }

    Token injecttoken(const Token& node) {
if(node.type !=TokenType::ROOT) return node;
  Token result; result.type = TokenType::STRING;
    for (const auto& child : node.children) {
        result.value += injecttoken(child).value;
    }
    

    return result; }

 
    
std::string runTokenFunc(const Token& node) {
      std::string result;
         auto it = funcHandlers.find(node.value); 
    if (it != funcHandlers.end()) {
        return it->second(node);  // 傳入整個 KEYWORD 節點
    }

    auto itk = tokenHandlers.find(node.value);
    if (itk != tokenHandlers.end()) {
        return "";  // 傳入整個 KEYWORD 節點
    }
 

     auto cusit =custom_command_arg.find(node.value); 
     if(node.type==TokenType::funcIDENTIFIER&&!node.children.empty()){
        if(cusit!=custom_command_arg.end()){  
             if(node.value.find("@")!=std::string::npos )return "";
            std::vector<Token> templa=cusit->second; 
            std::vector<size_t> args;std::string out,tmp;bool hasback=false;
            for(size_t pos=0;pos<templa.size();pos++){if(templa[pos].type==TokenType::INJECT)args.push_back(pos);}
            for(size_t i=0,j=0;i<node.children[0].children.size();i++){ Token cur=node.children[0].children[i];cur.value=singletoken(cur,",").value;
                if(cur.value!="<{"&&cur.value!="}>"){
                    if(cur.value==","||cur.value==")"){j++;tmp="";continue;}else{tmp+=" "+cur.value;}
                    if(j>=args.size()){j=0;hasback=true; }
                     if(cur.value=="{"){ for(auto& inside:cur.children)cur.value+=singletoken(inside,";").value;}
                   if(!hasback){templa[args[j]].value=tmp;} else{templa[args[j]].value+=tmp;}
                   
                }
            }
           for(auto& p:templa){out+=p.value;}
         return out;
        }else{return "";}
    }


    // 遞迴子 token
    for (const auto& child : node.children) {
        result += runTokenFunc(child);
    }


    return result;
}

// runToken 對應普通程式行
std::string runToken(const Token& node) {
    std::string result;
         auto it = tokenHandlers.find(node.value);
    if (it != tokenHandlers.end()) {
        return it->second(node);  // 傳入整個 KEYWORD 節點
    }

    auto itf = funcHandlers.find(node.value);//防止重複輸出runTokenFunc裡面{...}的關鍵字
    if (itf != funcHandlers.end()) {
        return "";  // 傳入整個 KEYWORD 節點
    }

    if(node.type==TokenType::funcIDENTIFIER&&!node.children.empty()){
       
     
        auto cusit =custom_command_arg.find(node.value);

        if( cusit!=custom_command_arg.end()){
            if(node.value.find("@")==std::string::npos )return "";
            std::vector<Token> templa=cusit->second; 
            std::vector<size_t> args;std::string out,tmp;bool hasback=false;
            for(size_t pos=0;pos<templa.size();pos++){if(templa[pos].type==TokenType::INJECT)args.push_back(pos);}
            for(size_t i=0,j=0;i<node.children[0].children.size();i++){ Token cur=node.children[0].children[i];cur.value=singletoken(cur,",").value;
                if(cur.value!="<{"&&cur.value!="}>"){
                    if(cur.value==","||cur.value==")"){j++;tmp="";continue;}else{tmp+=" "+cur.value;}
                    if(j>=args.size()){j=0;hasback=true; }
                     if(cur.value=="{"){ for(auto& inside:cur.children)cur.value+=singletoken(inside,";").value;}
                   if(!hasback){templa[args[j]].value=tmp;} else{templa[args[j]].value+=tmp;}
                   
                }
            }
           for(auto& p:templa){out+=p.value;}
         return out;
        }
               bool iftemplate=false; for(auto& child:node.children){  if(child.value!="(")iftemplate=true;}
             if(!iftemplate)result=node.value+"(" ;else  result=node.value; 
                for(auto& child:node.children){
                    if(child.value!="("){result+=singletoken(child,",").value+"(";continue;}
                    for(auto& p:child.children){
                    if(p.value=="("){result+=sumLeftparen(p);continue;}
                    result+=singletoken(p,",").value;
                }
                }
                
                return result+";";
            }

           if(node.type==TokenType::IDENTIFIER||node.type==TokenType::STRING||node.type==TokenType::NUMBER
                    ||node.type==TokenType::CHAR||node.type==TokenType::OPERATOR||node.type==TokenType::SEPARATOR){
                        
                         if(((node.value.find('.') != std::string::npos&&node.type!=TokenType::NUMBER) || (!node.children.empty() &&
         node.children.back().value.find('.') != std::string::npos) )) return "";//a.b.c節點跳過，另外處理
                        result+=node.value;
                    
                    }
            if(node.type==TokenType::NAMESPACELIKE||node.type==TokenType::STRUCTLIKE){return AdotBdotC(node);}

         if(node.value=="{"){result+=sumLeftbrackets(node);return result;}   
         if(what_is_struct_like.find(node.value)!=what_is_struct_like.end()){return node.value+" ";}
     


    // 遞迴子 token
       
    for (const auto& child : node.children) {
        result += runToken(child);
    }


    return result;
}

 

std::string escapeUtf8(const std::string& s) {
    std::string out;
    for(unsigned char c : s){
        if(c >= 32 && c <= 126) out += c; // 可打印 ASCII
        else {  char buf[5];snprintf(buf, sizeof(buf), "\\x%02X", c); out += buf; }}
    return out;
}


void printToken(const Token& node, int indent=0) {
    std::string pad(indent*2, '-');
    std::string value=((int)node.type==0)?"root---":escapeUtf8(node.value);
    std::cout << pad << "Token(type=" << (int)node.type
              << ", value=\""<< value
              << "\", line=" << node.line_number << ")\n";
     
    for(auto& child: node.children){
        printToken(child, indent+1);
    } 
  }  
  std::unordered_map<std::string, std::string> namespace_parent;
  std::unordered_map<std::string, std::string> struct_extension;
  std::unordered_map<std::string, bool> has_extension;
 
std::string namespace_tree(std::string name){
std::string result;
            if(namespace_parent.find(name)!=namespace_parent.end()) {result+= namespace_tree(namespace_parent[name])+"."+name;}
            else result+=name;
return result;
}
  
void registcondition(){
       registerToken("if", [](const Token& node) {
        std::string argcond,argcont,cur,cr,ed;
        for(auto& root: node.children){
            if(root.value=="("){ argcond="if(";
                for(auto& cond: root.children){
                    if(cond.type==TokenType::KEYWORD){
                        argcond="";argcond+=singletoken(cond,"").value;argcond+="if(";argcond+=cond.children[0].children[0].value;
                    }else{    cur= singletoken(cond,"").value;
                    if(cur=="("){for(auto& p:cond.children){cur+= singletoken(p,"").value;}}
                    argcond+=cur;}
                } 
            }
            if(root.value=="{"){ argcont="{";   
                for( size_t i=0; i<root.children.size(); i++){
                    auto& cont=root.children[i];
                    cr= singletoken(cont,";").value;
                    if(cont.value=="<{"||cont.value=="}>"||cont.type==TokenType::COMMENT){continue;}
                    if(cr=="("){for(auto& p:cont.children){cr+= singletoken(p,"").value;}}
                    argcont+=cr;
                    if(cont.type==TokenType::IDENTIFIER||cont.type==TokenType::STRING||cont.type==TokenType::NUMBER
                    ||cont.type==TokenType::CHAR||cont.type==TokenType::INJECT||cont.type==TokenType::OPERATOR||cont.value=="("
                    ||cont.type==TokenType::funcIDENTIFIER||cont.type==TokenType::NAMESPACELIKE||cont.type==TokenType::STRUCTLIKE
                    ){ if( root.children[i+1].value=="}"||root.children[i+1].line_number==cont.line_number+1){ argcont+=";\n";}
                        }   
                } }
        }
        return argcond+argcont+"\n";
    });
     registerToken("else", [](const Token& node) {
        std::string argcont,cur;
          
        for(auto& root: node.children){
             if(node.children.empty()){ argcont= "";}
            if(root.value=="{"){ argcont="{";   
                for( size_t i=0; i<root.children.size(); i++){
                    auto& cont=root.children[i];
                    cur= singletoken(cont,";").value;
                    if(cont.value=="<{"||cont.value=="}>"||cont.type==TokenType::COMMENT){continue;}
                    if(cur=="("){for(auto& p:cont.children){cur+= singletoken(p,"").value;}}
                    argcont+=cur;
                    if(cont.type==TokenType::IDENTIFIER||cont.type==TokenType::STRING||cont.type==TokenType::NUMBER
                    ||cont.type==TokenType::CHAR||cont.type==TokenType::INJECT||cont.type==TokenType::OPERATOR||cont.value=="("
                    ||cont.type==TokenType::funcIDENTIFIER||cont.type==TokenType::NAMESPACELIKE||cont.type==TokenType::STRUCTLIKE
                    ){ if( root.children[i+1].value=="}"||root.children[i+1].line_number==cont.line_number+1){ argcont+=";\n";}
                        }   
                } }
        }
        return "else " + argcont + "\n";
    });
        registerToken("for", [](const Token& node) {
        std::string argcond,argcont,cur,cr,ed;std::vector<char> sepg;int sep=0;
        for(auto& root: node.children){
            if(root.value=="("){ argcond="for(";
                for(auto& cond: root.children){
                    if(cond.value==";"||cond.value==","){sepg.push_back(cond.value[0]);if(cond.value==";")sep++;}
                   cur= singletoken(cond,";").value;
                    if(cur=="("||cur=="{"){for(auto& p:cond.children){cur+= singletoken(p,",").value;}}
                    if(cond.type==TokenType::IDENTIFIER){cur=" "+cur;}
                    argcond+=cur;
                }
                if(sep==2){
                    for(size_t i=0,j=0; i<argcond.size(); i++){ if(argcond[i]==';'){ if(sepg[j]==','){argcond[i]=',';} j++; }}
                } 
            }
            if(root.value=="{"){ argcont="{";   
                for( size_t i=0; i<root.children.size(); i++){
                    auto& cont=root.children[i];
                    cr= singletoken(cont,";").value;
                    if(cont.value=="<{"||cont.value=="}>"||cont.type==TokenType::COMMENT){continue;}
                    if(cr=="("){for(auto& p:cont.children){cr+= singletoken(p,"").value;}}
                    argcont+=cr;
                    if(cont.type==TokenType::IDENTIFIER||cont.type==TokenType::STRING||cont.type==TokenType::NUMBER
                    ||cont.type==TokenType::CHAR||cont.type==TokenType::INJECT||cont.type==TokenType::OPERATOR||cont.value=="("
                    ||cont.type==TokenType::funcIDENTIFIER||cont.type==TokenType::NAMESPACELIKE||cont.type==TokenType::STRUCTLIKE
                    ){ if( root.children[i+1].value=="}"||root.children[i+1].line_number==cont.line_number+1){ argcont+=";\n";}
                        }   
                } }
        }
        return argcond+argcont+"\n";
    });

    registerToken("while", [](const Token& node) {
        std::string argcond,argcont,cur,cr,ed;std::vector<char> sepg;int sep=0;
        for(auto& root: node.children){
            if(root.value=="("){ argcond="while(";
                for(auto& cond: root.children){
                    if(cond.value==";"||cond.value==","){sepg.push_back(cond.value[0]);if(cond.value==";")sep++;}
                   cur= singletoken(cond,";").value;
                    if(cur=="("||cur=="{"){for(auto& p:cond.children){cur+= singletoken(p,",").value;}}
                    if(cond.type==TokenType::IDENTIFIER){cur=" "+cur;}
                    argcond+=cur;
                }
                if(sep==2){
                    for(size_t i=0,j=0; i<argcond.size(); i++){ if(argcond[i]==';'){ if(sepg[j]==','){argcond[i]=',';} j++; }}
                } 
            }
            if(root.value=="{"){ argcont="{";   
                for( size_t i=0; i<root.children.size(); i++){
                    auto& cont=root.children[i];
                    cr= singletoken(cont,";").value;
                    if(cont.value=="<{"||cont.value=="}>"||cont.type==TokenType::COMMENT){continue;}
                    if(cr=="("){for(auto& p:cont.children){cr+= singletoken(p,"").value;}}
                    argcont+=cr;
                    if(cont.type==TokenType::IDENTIFIER||cont.type==TokenType::STRING||cont.type==TokenType::NUMBER
                    ||cont.type==TokenType::CHAR||cont.type==TokenType::INJECT||cont.type==TokenType::OPERATOR||cont.value=="("
                    ||cont.type==TokenType::funcIDENTIFIER||cont.type==TokenType::NAMESPACELIKE||cont.type==TokenType::STRUCTLIKE
                    ){ if( root.children[i+1].value=="}"||root.children[i+1].line_number==cont.line_number+1){ argcont+=";\n";}
                        }   
                } }
        }
        return argcond+argcont+"\n";
    });
    registerToken("extension_slot", [](const Token& node){std::string result="";
        for(auto& child:node.children){
            if(child.value=="("){
                for(auto& ext:child.children)if(ext.type==TokenType::STRING&&struct_extension.count(ext.value))
 result=struct_extension[ext.value];
            }
        } 
        return result;}); 
    
}

void registfunckeyword(){
    registerfunc("function",[](const Token& node){
        std::string parag,type,funcname,cont,cr;bool declared=false,guesstype=false,hasparen=false;
        for(auto& p:node.children){
            if(p.type==TokenType::TYPE){type=p.value;type=(type=="float")?"double":(type=="string")?"std::string":type;}
            if(p.type==TokenType::NAMESPACELIKE){type=singletoken(p,"").value;}
            if(p.type==TokenType::funcIDENTIFIER){funcname=p.value;}
            if(p.value=="("){parag="("; hasparen=true;  
                for(auto& child:p.children){ if(child.value=="<{"||child.value=="}>"||child.type==TokenType::COMMENT){continue;}
                      parag+=singletoken(child,",").value+" ";}
            }
            if(p.value=="{"){ cont="{"; declared=true;
                for( size_t i=0; i<p.children.size(); i++){
                    auto& child=p.children[i];
                    if(child.value=="@function") continue;
           
                         cr= singletoken(child,";").value;
                    if(child.value=="<{"||child.value=="}>"||child.type==TokenType::COMMENT){continue;}
                    if(cr=="("){for(auto& p:child.children){cr+= singletoken(p,"").value;}}
                    if(cr=="return"){cr+=" ";guesstype=true;}
                    cont+=cr;
                    if(child.type==TokenType::IDENTIFIER||child.type==TokenType::STRING||child.type==TokenType::NUMBER
                    ||child.type==TokenType::CHAR||child.type==TokenType::INJECT||child.type==TokenType::OPERATOR||child.value=="("
                    ||child.type==TokenType::funcIDENTIFIER||child.type==TokenType::NAMESPACELIKE||child.type==TokenType::STRUCTLIKE
                    ){ if( p.children[i+1].value=="}"||p.children[i+1].line_number==child.line_number+1){ cont+=";\n";}
                        
                        } 
                    
                     
                } }
            if(p.value=="["){
           
                type="";funcname="";parag="";cont="";hasparen=false;
                 
            }

        }
        if(type==""&&hasparen){type="void";if(guesstype){type="auto";}
    if(!declared&&hasparen){cont=";";}
    } 
         
        

        return type+" "+funcname +parag+cont+"\n";
    });
     registerfunc("@function",[](const Token& node){
        std::string rs;
        for(auto&p:node.children.back().children){
            if(p.type==TokenType::INJECT) rs+=p.value;
        }
        return rs+"\n";
     });
      registerfunc("namespace",[](const Token& node){
         std::string result,namespa; 
        
         for(auto child:node.children){
            if(child.type==TokenType::NAMESPACELIKE){namespa=child.value;}
            if(child.value=="{"){ result="namespace "+namespa+"{";   
                for( auto& cont:child.children){
                    if(cont.value=="namespace"&&!cont.children.empty()){namespace_parent[cont.children[0].value]=namespa;}

                    if(cont.value=="@custom"&&!cont.children.empty()){ 
                        cont.children[0].value=namespace_tree(namespa)+"."+cont.children[0].value;}
                        //多namespace定義@custom
                    if(custom_command_arg.find(namespace_tree(namespa)+"."+cont.value)!=custom_command_arg.end()) {cont.value=namespace_tree(namespa)+"."+cont.value;}
                      result+=singletoken(cont,";").value;
                      //多namespace呼叫@custom生成的api
                } }
         }
        return result+"\n";
     });
      registerfunc("struct",[](const Token& node){
        std::string result,struc;bool isDerive=false;bool isExten=false;
        int strucnum=0;
         for(auto& child:node.children){
            if(child.type==TokenType::STRUCTLIKE||child.type==TokenType::TYPE){
                if(child.value=="derive"&&strucnum==1){isDerive=true;struc+=":";}
                if(child.value=="extension"&&strucnum==0){isExten=true;}
                if(!isDerive&&!isExten){ struc=child.value;strucnum++;}}
            if(child.value=="("&&isDerive){ 
                for(auto& cond: child.children){
     if(cond.value=="<{"||cond.value=="}>"){continue;}
      if(cond.value!=","&&cond.value!=")")struc+="public "+singletoken(cond,";").value;
      else if(cond.value==",")struc+=",";
            }
        }
        if(child.value=="("&&isExten){ 
                for(auto& cond: child.children){
      if(cond.type==TokenType::STRING)struc=cond.value;
            }
        }
            if(child.value=="{"&&!isExten){ result="struct "+struc+"{";   
                for( auto& cont:child.children){
                      result+=singletoken(cont,";").value;
                } result+=";"; }
            if(child.value=="{"&&isExten){
                  for( auto& cont:child.children) if(cont.value!="}") result+=singletoken(cont,";").value;  }
         }
         if(isExten){if(!has_extension[struc+result])
            struct_extension[struc]+=result;has_extension[struc+result]=true;result="";}
        return result+"\n";
     });
     registerfunc("@custom",[](const Token& node){
        std::vector<Token> args;
         for(auto child:node.children[0].children[0].children){
            if(child.type==TokenType::INJECT) {args.push_back(child); }
            if(child.type==TokenType::STRING){child.value=child.value.substr(1,child.value.size()-2);args.push_back(child); }
         }
         if(!args.empty()) custom_command_arg[node.children[0].value]=args;
        return "";
     });
     registerfunc("use",[](const Token& node){
         std::string result,scope,usecustom;bool notusingname=false;
          auto trim = [](std::string s) {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
        return s;
    };
         for(auto& child:node.children){
            std::stringstream ss(child.value);
            while (getline(ss, scope, ',')){scope=trim(scope); notusingname=false; 
              for(auto& [k,v]:custom_command_arg){  
                if(k.compare(0,scope.size()+1,scope+".")==0) {
                    custom_command_arg[k.substr(scope.size()+1)]=v;}
              }
                for(size_t i=0;i<scope.size();i++){
                    if(scope[i]=='.'){scope[i]=':';scope.insert(i+1,":");}
                    if(scope[i]=='-'&&scope[i+1]=='>'){scope[i]=':';scope[i+1]=':';notusingname=true;}
                }
                if(notusingname) { 
                result+="using  "+scope+";\n";}
                else{
                    result+="using namespace "+scope+";\n";
                }
                
    
                }
            }
        return result;
     });
    
 
     registerfunc("package",[](const Token& node){  return "";});

}
void find_slot(const Token& node){
    Token ext=node;
     if(ext.value=="struct"&&ext.children.size()>2&&ext.children[0].value=="extension") runTokenFunc(ext);
        for (const auto& child : node.children) {
        find_slot(child);
    }
}
// ======token區域=======
//註解
bool isComment(const std::string& line) {
   bool in_string = false;
    bool escape = false;

    for (size_t i = 0; i + 1 < line.size(); ++i) {
        char c = line[i];

        if (escape) {
            escape = false;
            continue;
        }

        if (c == '\\') {
            escape = true;
            continue;
        }

        if (c == '"') {
            in_string = !in_string;
            continue;
        }

        // 第一個非空白字元前遇到 //
        if (!in_string) {
            if (c == ' ' || c == '\t')
                continue;

            if (c == '/' && line[i + 1] == '/')
                return true;

            // 一旦遇到有效字元，就不可能是註解行，直接返回 false所以不會跑下一個字
            return false;
        }
    }
    return false;
}


    
size_t findMatchingEnd(const std::string& line, size_t start, const std::string& h, const std::string& e) {
    int depth = 1;
    // 確保 start 不越界
    if (start >= line.size()) return std::string::npos;
    
    for (size_t i = start; i + e.length() <= line.size(); ++i) {
        // 先檢查是否遇到新的起始符號 (嵌套)
        if (line.compare(i, h.length(), h) == 0) {
            depth++;
            i += h.length() - 1; // 跳過符號長度
        }
        // 再檢查是否遇到結束符號
        else if (line.compare(i, e.length(), e) == 0) {
            depth--;
            if (depth == 0)
                return i;
            i += e.length() - 1; // 跳過符號長度
        }
    }
    return std::string::npos;
}
// 萃取 print("abc",1,2) 裡面的參數部分以及其他() 內容
std::string extractArgs(const std::string& line, const std::string& h, const std::string& e, bool& keywordEnd) {
    keywordEnd = true; // 預設為已結束
    std::string_view svline(line);
    size_t l = svline.find(h);
    
    // 如果連頭都找不到，直接回傳空或原字串（視需求而定，這裡假設若沒頭則不處理）
    if (l == std::string::npos) {
        keywordEnd = true; // 沒頭不算未完成，算找不到
        return ""; 
    }

    // 尋找對應的結尾
    size_t r = findMatchingEnd(line, l + h.length(), h, e);

    if (r == std::string::npos) {
        // 找不到結尾，表示多行模式，回傳 header 之後的所有內容
        keywordEnd = false;
        return line.substr(l + h.length());
    } else {
        // 找到結尾，回傳中間的內容
        return line.substr(l + h.length(), r - (l + h.length()));
    }
}

// 通用呼叫，用於主迴圈：直接給一行程式碼，它會自動選擇指令
// 通用呼叫，用於主迴圈：直接給一行程式碼，它會自動選擇指令
std::string runCommand(const std::string& line,bool iffunc) {
    // [新增] 靜態變數，用於記憶多行狀態
    static std::string pendingBuffer = "";     // 累積的程式碼
    static std::string pendingCmdKey = "";     // 正在等待的指令關鍵字 (如 print)
    static std::string pendingHead = "";       // 正在等待的起始符 (如 "(")
    static std::string pendingEnd = "";        // 正在等待的結束符 (如 ")")
    // 假設 cpsCommands 值的型別是 std::function<std::string(std::string)>
    static std::function<std::string(std::string)> pendingFunc = nullptr; 
    // 判斷是否處於多行模式
    bool inMultiLine = !pendingBuffer.empty();
    // 如果是註解中，或是空行，且不在多行模式下，直接略過
    std::string_view svline(line);
    if (!inMultiLine && (svline.find("//") == 0 || line.empty())) return ""; 
    // 注意：原本的 commentInReg 若是全域變數請保留使用，此處僅示範邏輯
    std::string currentLine;
    if (inMultiLine) {
        // 多行模式：將新的一行接在緩衝區後面 (加上換行符號)
        pendingBuffer += "\n" + line; currentLine = pendingBuffer;
    } else {   currentLine = line;  }
    // 如果在多行模式，直接使用記憶中的參數進行檢查，不需要重跑迴圈
    if (inMultiLine) {
        bool keywordEnd = false;
        // 嘗試在累積的字串中萃取參數
        std::string args = extractArgs(currentLine, pendingHead, pendingEnd, keywordEnd);
        if (keywordEnd) {
            // 找到了結尾！執行指令
            std::string result = pendingFunc(args);
            // 清空靜態狀態，回到單行模式
            pendingBuffer = ""; pendingCmdKey = "";   pendingHead = "";    pendingEnd = ""; pendingFunc = nullptr;
            return result;
        } else {  return "";     }  }
    // --- 以下為新指令的偵測邏輯 (原本的迴圈) ---

    for (auto& p : cpsCommands) {
        // 優化：先檢查是否包含指令，避免無效搜尋
        size_t cmdPos = svline.find(p.first); if (cmdPos == std::string::npos) continue;
        // 確保指令不是變數的一部分 (簡單邊界檢查，可選)
        // if (cmdPos > 0 && isalnum(line[cmdPos-1])) continue; 
        if (commentInReg) return ""; // 假設這是全域變數
        for (auto& note : func_head_end) {
            const std::string& hd = note.first; const std::string& ed = note.second;
            // 檢查這一行是否有對應的起始符號 (例如 "(")
            if (svline.find(hd) == std::string::npos) continue;
            bool isFuncCmd = (p.first == "@function");
            bool shouldRun = (isFuncCmd && iffunc) || (!isFuncCmd && !iffunc);
            bool keywordEnd = false;std::string args = extractArgs(line, hd, ed, keywordEnd);
            if (!keywordEnd) {
                // [變更] 發現沒閉合，進入多行模式
                if (shouldRun) {
                pendingBuffer = line;   // 存入緩衝區
                pendingCmdKey = p.first; 
                pendingFunc = p.second; // 記住要執行的函數
                pendingHead = hd;       // 記住符號
                pendingEnd = ed;
                return ""; // 等待下一行
                }
            } else {
                // 單行直接完成，執行並回傳
                 if (shouldRun) {
            return p.second(args);
        }
        return ""; // 雖然解析成功，但當前模式不執行
            } }  }
    return ""; // 找不到指令就忽略
}


// 讀 ini 檔，用逗號分割，轉成 -I 或 -L 參數
std::string parseIni(const std::string& path, const std::string& flag) {
    std::ifstream infile(path);
    if (!infile) return "";

    std::string line; 
    std::string result;
    while(std::getline(infile, line)){
    std::stringstream ss(line);
    std::string token;
    while (std::getline(ss, token, ',')) {
        if (!token.empty()) {
             result +=  flag+ "\"" + token + "\" ";
             
        }
    }
    
    }
    return result;
}
// 判斷字串是否為布林值
auto is_bool = [](const std::string& s){
    return s == "true" || s == "false";
};
auto is_number = [](const std::string& s){
    char* end;
    std::strtod(s.c_str(), &end);
    return *end == '\0';
};
 // 去掉前後空白
    auto trim = [](std::string s) {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
        return s;
    };

std::unordered_set<std::string> cppsp_module;//紀錄已載入的模組，避免重複載入
std::vector<std::string> mod_import_line,originLoadOrder;//儲存模組中原始import a,b,c

  std::unordered_map<std::string, std::vector<std::string>> depGraph;//紀錄模組依賴關係圖
             std::unordered_set<std::string> graphBuilt;//紀錄已經紀錄依賴的模組，避免重複構建
            
             std::unordered_map<std::string,int> visitState;//拓樸排序訪問狀態，0=未訪問，1=訪問中，2=已訪問 

             std::vector<std::string> loadOrder;//存儲最終的載入順序
             std::unordered_map<std::string,fs::path> modpath_cache;//模組路徑快取，避免重複尋找同一模組
 
             
     fs::path pathByimport(const std::string& mod,const fs::path basedir){
        std::string path = mod;
 for(size_t j=0;j<path.size();j++)if(path[j]=='.')path[j]=(isWindows)?'\\':'/';
return fs::path(basedir) /(path+".cppsp");
     }

 void buildGraph(const fs::path& modpath,const std::string& topmodname,const fs::path& pathInini){
     
     if(graphBuilt.count(topmodname)) return; // 已經構建過了
     if(!fs::exists(modpath)){ 
        return;}  
    
 

        graphBuilt.insert(topmodname); depGraph[topmodname]={};visitState[topmodname]=0;
        cppsp_module.insert(topmodname);//std::cerr<<"suceessfully loaded "<<topmodname<<'\n';
            std::ifstream modfile(modpath); Token modtoken =tokenizeFile(modfile,1); 
            if(!modfile){ std::cerr<<"Error: Failed to open module file "<<modpath<<"\n"; return;}
            modpath_cache[topmodname]=modpath;
         
            for(auto& child:modtoken.children){
                if(child.value=="import"&&!child.children.empty()){std::string modname;std::stringstream ss(trim(child.children[0].value));
                    while(std::getline(ss,modname,',')){modname=trim(modname);
                      fs::path deepermodpath=pathByimport(modname,pathInini);
                       if(fs::exists(deepermodpath)){ depGraph[topmodname].push_back(modname);
                        buildGraph(deepermodpath,modname,pathInini); }
                       else{ return;}
                    }
                }
              }  
 
    
    }  
    bool topoDFS(const std::string& mod){
 
        switch(visitState[mod]){
     case 0:
         visitState[mod]=1;
         for(auto& dep:depGraph[mod]){
           if(!topoDFS(dep)) return false;
         }
         loadOrder.push_back(mod);
         visitState[mod]=2;return true;
     case 1:
        std::cerr<<"Error:"<<mod<<"has circular dependency\n";
        return false;
     case 2:
         return true;
        }
 return true;
    }
 
    //處理模組
std::string moduleIni(const std::string& path,const Token& maincppsp) {
  std::ifstream infile(path); 
    if (!infile) return {};
 
  std::vector<std::string> record_modules;//紀錄
std::unordered_set<std::string>ready_for_loading_modules,deduplicated;// 紀錄.cppsp檔案模組
    for(auto& child:maincppsp.children){
        if(child.value=="import"&&!child.children.empty()) {std::stringstream ss(child.children[0].value);std::string importss;
            while (std::getline(ss, importss, ',')){ record_modules.push_back(trim(importss));
    }  }    }
   
 
    std::string line; 
    std::string result;
    while(std::getline(infile, line)){
    std::stringstream ss(line);  std::string pathInini;
    while (std::getline(ss, pathInini, ',')) {
        if (!pathInini.empty()) {pathInini=trim(pathInini);
           

    for(size_t i=0;i<record_modules.size();i++){ 
            fs::path modpath=pathByimport(record_modules[i],fs::path(pathInini) );

            if(fs::exists(modpath)){if(deduplicated.count(record_modules[i])==0) {originLoadOrder.push_back(record_modules[i]);}
                deduplicated.insert(record_modules[i]);}

             buildGraph(modpath,record_modules[i],fs::path(pathInini));
           }
  
        }
    }
//跑完所有ini中路徑才開始拓樸排序，確保依賴圖完整 
    }
    for(auto& mod:originLoadOrder){
topoDFS(mod);
    }
    
    for(auto& mod:loadOrder){/*std::cerr<<mod<<":{";
          for(auto& dep:depGraph[mod]){std::cerr<<dep<<",";}std::cerr<<"}\n";   */
   // std::cerr<<modpath_cache[mod]<<'\n';
          std::ifstream modfile(modpath_cache[mod]); std::ostringstream readmodcont;readmodcont<<modfile.rdbuf();std::string modspace=mod;
          std::istringstream modtokenline(readmodcont.str());
           Token modtoken =tokenizeFile(modtokenline,1);
          for(auto& child:modtoken.children){if(child.value=="import"&&!child.children.empty())mod_import_line.push_back(child.children[0].value);
            if(child.value=="package"&&!child.children.empty())modspace=child.children[0].value;
        }
          std::string modnamesapce,finaltoken;std::stringstream ss(modspace);
         finaltoken=readmodcont.str();
         std::vector<std::string> nameorder;
          while(std::getline(ss,modnamesapce,'.')){nameorder.push_back(modnamesapce);}
          for(int i=nameorder.size()-1;i>=0;i--){
            finaltoken="namespace "+nameorder[i]+"{"+finaltoken+"}\n";
          }
          
           result+=finaltoken;
   
    
    }
    return result;
}

std::string parse_utf8(const std::string &s) {
    std::string r;
    for(size_t i=0;i<s.size();)
        if(s[i]=='\\'&&i+3<s.size()&&s[i+1]=='x')
            r.push_back(static_cast<char>(std::stoi(s.substr(i+2,2),nullptr,16))),i+=4;
            //16代表把字串當16進位看
        else
            r.push_back(s[i++]);
    return r;
}
std::wstring to_wide(const std::string& utf8_str) {
    return std::filesystem::path(utf8_str).wstring();
}
#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>
    std::string getargv_path(const std::string& arv,int pos){
int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::string result=std::filesystem::path(argv[pos]).u8string();
    LocalFree(argv); 
    return result;
    }
    #else 
    std::string getargv_path(const std::string& arv,int pos){
 return arv;
    }
#endif
 
int main(int argc, char* argv[]) {
    bool enableclang =false;bool skipcompile=false;bool enableoverwrite = false;
    #if defined(_WIN32) || defined(_WIN64) 
            SetConsoleCP(65001);SetConsoleOutputCP(65001);
              #else
            //have no requirement for other platforms theorically
             #endif
     
    //註冊
 
 registfunckeyword();   

     registerToken("import", [](const Token& node){return "";});registerToken("#usegcc", [](const Token& node){return "";});registerToken("#useclang", [](const Token& node){return "";});   
     registerToken("@command", [](const Token& node){return "";}); registerToken("#overwrite", [](const Token& node){return "";}); 
     registerToken("#skipcompile", [](const Token& node){return "";}); 
      registerToken("var", [](const Token& node){
             std::string arga,type,optrar,cur,botf;int vat=1;std::vector<std::string> n;std::vector<Token> c;
             Token nodes=node;
             type=nodes.children.back().value;
           /*  Token nspce_type = node.children.back();bool isnsType=false; //指標必免值覆蓋
            if(nodes.children.back().type==TokenType::NAMESPACELIKE){ std::cout<<"gyhuji\n";
while (!nspce_type.children.empty()) {  nspce_type = nspce_type.children.back(); }
           
if(nspce_type.type==TokenType::TYPE) {isnsType=true;type=nspce_type.value;nodes.children.pop_back();} 
}*/
              if(nodes.children.back().type==TokenType::NAMESPACELIKE) nodes.children.back().type=TokenType::TYPE;
             for (auto child : nodes.children){
                
                if(child.type==TokenType::OPERATOR){optrar=child.value;}
                if(child.type==TokenType::funcIDENTIFIER){c.push_back(singletoken(child,","));}
                if(child.type==TokenType::IDENTIFIER){
                    if(optrar=="") n.push_back(child.value);else c.push_back(child);
                }
                if(child.type==TokenType::NAMESPACELIKE||child.type==TokenType::STRUCTLIKE){ 
                  //  if(isnsType) continue;
                    if(optrar=="") {n.push_back(AdotBdotC(child));}else{Token dotchain={TokenType::IDENTIFIER,AdotBdotC(child),child.line_number,{}};c.push_back(dotchain);}
                } 
                else if(child.type==TokenType::STRING||child.type==TokenType::NUMBER||child.type==TokenType::CHAR||child.type==TokenType::INJECT){
                    c.push_back(child);}
                else if(child.value=="{"){cur="{";
                    for(auto& p:child.children){
                       
                        if(p.type==TokenType::funcIDENTIFIER){cur+=singletoken(p,",").value+" ";}
                        else {  botf=singletoken(p,",").value;if(botf=="\"ture\"")botf="true";if(botf=="\"false\"")botf="false";
                            cur+=botf;}
                        
                      } 
                    c.push_back({TokenType::STRING,cur,child.line_number,{}});cur="";n[c.size()-1]+="[]";}
                if(child.type==TokenType::SEPARATOR)vat++;     
             }
             for(size_t i=0;i<n.size();i++){
                if(optrar=="="||optrar==""){
                              if(n[i].empty())  break;
                 std::string val = (i < c.size()) ? c[i].value : ""; 
                 std::string op = val.empty() ? "" : optrar;
                 if(type=="string"){arga+=(Ifiostream)?"std::string "+n[i]+op+val+";":"char "+n[i]+"[]"+op+val+";";}
                 else if(type=="int"){arga+="int "+n[i]+op+val+";";}
                 else if(type=="float"){arga+="double "+n[i]+op+val+";";}
                 else if(type=="bool"){arga+="bool "+n[i]+op+val+";";}
                 else if(type=="char"){arga+="char "+n[i]+op+val+";";}
                 else if(what_is_struct_like.find(type)!=what_is_struct_like.end()||what_is_namespace_like.find(type)!=what_is_namespace_like.end()){
                    if(n[i].find("[]")!=std::string::npos){ n[i].pop_back();n[i].pop_back();}
                   //  if(false&&node.children.back().type==TokenType::NAMESPACELIKE){
                   //      arga+=AdotBdotC(node.children.back())+" "+n[i]+op+val+";";
                  //   }else{
                    arga+=type+" "+n[i]+op+val+";";}
                   //  }
 
                }else{
                    if(n[i].empty())  break;
                    
                 std::string val = (i < c.size()) ? c[i].value : "";
                 std::string op = val.empty() ? "" : optrar;
                 arga+=" "+n[i]+op+val+";";
                }
      
             }
          
        
        return arga+"\n";});
     registcondition();
 
   
    registerToken("println", [](const Token& node) {
    std::string args,cur; 
    
     for (auto& child : node.children) {
            cur= mergetoken(child,");\nprintf(").value; 
            args += cur;
    }
    return "printf(" + args+");\n";
});

  registerToken("print", [](const Token& node) {
    std::string args,cur,prenum; Token curtoken;args="";bool opt=false; 
    std::string ifio=(Ifiostream)?";\n":");\n";int i=0;
    const auto& root= node.children[0].children;//切換到'('或'{'後面的節點
     for (const auto& child : root) {
            curtoken= singletoken(child,""); cur=curtoken.value;
             if(curtoken.type==TokenType::NUMBER){ if(!opt){
                  if(stod(curtoken.value)==(int)stod(curtoken.value)){args+=(Ifiostream)?"std::cout<<"+cur:" printf(\"%d\","+cur;}
                  else args+= (Ifiostream)?"std::cout<<"+cur:" printf(\"%g\","+cur;}  else{args+=cur;} }
             if(curtoken.type==TokenType::SEPARATOR) {  args+=ifio;opt=false;}     
             if(curtoken.value[0]=='L') {(Ifiostream)?args+="std::wcout<<"+cur :"wprintf("+cur;}else{
               if(curtoken.type==TokenType::STRING||curtoken.type==TokenType::CHAR){ args+=(Ifiostream)?"std::cout<<"+cur:"printf(" + cur;}}
             if(curtoken.type==TokenType::OPERATOR){  args += cur;opt=true;}
             if(curtoken.type==TokenType::INJECT){ args+=(Ifiostream)?"std::cout<<"+cur:"printf(" + cur;}
             if(curtoken.type==TokenType::IDENTIFIER){
                if(!opt){args+=(Ifiostream)?"std::cout<<"+cur:"printf("+cur;} else{args+=cur;}}
             if(child.type==TokenType::NAMESPACELIKE||child.type==TokenType::STRUCTLIKE){
                if(!opt){args+=(Ifiostream)?"std::cout<<"+AdotBdotC(child):"printf("+AdotBdotC(child);}else{args+=AdotBdotC(child);} }
            if(child.type==TokenType::KEYWORD){
                         args+=singletoken(child,"").value;std::string varinside=child.children[0].children[0].value;
                         args+=(Ifiostream)?"std::cout<<"+varinside:"printf("+varinside;
                    }
            if(child.type==TokenType::funcIDENTIFIER){
                if(i==0)args="";
                args+="std::cout<<"+cur;    
            }
                
               
    i++;}
    return args+ifio;
});
registerToken("input",[](const Token& node){
    std::string args;
    const auto& root= node.children[0].children;//切換到'('或'{'後面的節點
      if(Ifiostream==false) args= "printf(\"need import iostream\")";
     for (const auto& child : root) {
        if(child.type==TokenType::IDENTIFIER) args+="std::cin>>"+child.value+";\n";
        if(child.type==TokenType::INJECT) args+="std::cin>>"+child.value+";\n";
        if(child.type==TokenType::NAMESPACELIKE||child.type==TokenType::STRUCTLIKE){args+="std::cin>>"+ AdotBdotC(child)+";\n";}
        
    }
    return  args;
});
registerToken("@inject",[](const Token& node){
    int cont;  
    std::string args,cur; const auto& root= node.children[0].children; 
     for (auto& child : root) {
            cur= injecttoken(child).value; 
            if(child.type == TokenType::STRING) args += cur;
             
    }
    if (args.size() >= 2 && args.front() == '"' && args.back() == '"') {
    args = args.substr(1, args.size() - 2);
}else{
    args ="";
}
    return  args;
});
 
 
registerCommand("@inject","(",")",  [](const std::string& args) {
    std::string o="";
     if (args.size() >= 2 && args.front() == '"' && args.back() == '"') {
      return o;
}
    return args;
});
registerCommand("@function","<<",">>",  [](const std::string& args) {
     return args;
});
////////
    if (argc < 2) {
        std::cerr << "Usage: cppsp_compiler(if not in environment path:.\\cppsp_compiler.exe or c:\\...\\cppsp_compiler.exe) script.cppsp\ninclude.ini:C:\\...\\include1,c:\\...\\include2\nlib.ini:C:\\...\\lib1,c:\\...\\lib2\n";
        std::cerr << "\33[93m(Optional) rename cppsp_compiler.exe(or cppsp_compiler) to any name you like to change compile command like:\33[0m\33[36mcppsp,abcdef....\33[0m\n";
        return 1;
    }

    bool gen_header=false;
    if(argc>2){  
        for(int i=1;i<argc;i++){
            if(strcmp(argv[i],"-header")==0){  gen_header=true;break;}
            
        }
        if(strcmp(argv[1],"new")==0){std::string newfname =getargv_path(argv[2],2)+".cppsp";
            if(!fs::exists("include.ini")){std::ofstream inc("include.ini");inc<<fs::current_path().string();inc.close();}
            if(!fs::exists("lib.ini")){std::ofstream inc("lib.ini");inc<<fs::current_path().string();inc.close();}
            if(!fs::exists("module.ini")){std::ofstream inc("module.ini");inc<<fs::current_path().string();inc.close();}
            if(!fs::exists(newfname)){fs::path incpa(newfname);std::ofstream inc(incpa );inc.close();}
                  std::cout<<"Create new project: "<<newfname<<"\n";
                   return 0;}
    }

  //預留模組安裝功能
  /*  if(strcmp(argv[1],"install")==0){
        for(int i=2;i<argc;i++){
         size_t pos = std::string(argv[i]).find("-");std::string mod;
         std::string modname=std::string(argv[i]).substr(0,pos);
         std::string modver=std::string(argv[i]).substr(pos+1);
            if(std::string(argv[i])=="cppsp"){
                if(isLinux)    mod="curl -L -o cppsp_compiler https://github.com/user19870/cppsp/raw/refs/heads/First/cppsp_compiler_linux.delete_linux";
                else if(isMac)  mod= " curl -L -o cppsp_compiler https://github.com/user19870/cppsp/raw/refs/heads/First/cppsp_compiler_mac.delete_mac";
                else  mod="curl -L -o cppsp_compiler.exe https://github.com/user19870/cppsp/raw/refs/heads/First/cppsp_compiler.exe";
            system(mod.c_str());
            if(isLinux||isMac) system("chmod +x cppsp_compiler");
            }else{
                   mod="curl -L -o \""+std::string(argv[i])+".zip\""+" \"https://github.com/user19870/cppsp/moduldes/"+modname+"/"+std::string(argv[i])+".zip";
                 system(mod.c_str());
                system(("tar -xf \""+std::string(argv[i])+".zip\"").c_str()); }
            } return 0;
    }else if(strcmp(argv[1],"update")==0){
         for(int i=2;i<argc;i++){

    }  
     return 0;} 
*/
  std::string __u8path=getargv_path(argv[1],1); __u8path=parse_utf8(__u8path);
  ; fs::path cpsPath(__u8path);
    if (!fs::exists(cpsPath)) {
       std::cerr << "File not found.\n";
        return 1;
    }

    std::ifstream infile(cpsPath);std::ifstream funcfile(cpsPath);
    if (!infile) {
        std::cerr << "Cannot open file.\n";
        return 1;
    }

    fs::path cppPath = cpsPath.parent_path() / (cpsPath.stem().concat(".cpp"));
    if(gen_header) cppPath = cpsPath.parent_path() / (cpsPath.stem().concat(".h"));
    std::ofstream outfile(cppPath);
    
    if (!outfile) {
        std::cerr << "Cannot create cpp file.\n";
        return 1;
    }

    std::ifstream orgifile(cpsPath);
    std::ostringstream tokencontent;tokencontent<<orgifile.rdbuf();
     
    std::istringstream tokenfile(tokencontent.str());
   Token root = tokenizeFile(tokenfile,1);

   std::string modulefolder = moduleIni("module.ini",root);
  
   std::istringstream finalfile(modulefolder+tokencontent.str());
   root=tokenizeFile(finalfile,1);
 
    tokenstream.push_back(root);

  

std::unordered_set<std::string> includedHeaders; // 紀錄已包含的標頭，避免重複
    outfile << "#include <stdio.h>\n";
    std::string importline; std::ifstream origfile(cpsPath);
 
    std::ostringstream ss;ss<<origfile.rdbuf();std::string finalcont=ss.str();
    for(auto& val:mod_import_line){finalcont="import "+val+"\n"+finalcont;}
    std::istringstream fileinclude(finalcont);

while (std::getline(fileinclude, importline)) {
  bool comment=isComment(importline);std::string svimportline=importline;
if (!comment && svimportline.find("import ") != std::string::npos) {
    size_t pos = svimportline.find("import ");
    std::string imports = importline.substr(pos + 7); // "import " 長度 7
  
    imports = trim(imports);

    // 拆成多個標頭
    std::stringstream ss(imports);
    std::string header;
    while (std::getline(ss, header, ',')) {
        header = trim(header); // 每個標頭也要去掉空白
        if (header.empty()||includedHeaders.count(header)) continue;
        includedHeaders.insert(header); // 紀錄已包含的標頭
         

        fs::path importFile = cpsPath.parent_path() / header;
        std::string import= importFile.lexically_relative(cpsPath.parent_path()).string();
    
      
         if(cppsp_module.count(header))continue;

        if(header[0]=='\"'&&header[header.length()-1]=='\"') import=import;
        else if(header[0]=='<'&&header[header.length()-1]=='>') import=import;
        else import="\""+import+"\"";
        outfile << "#include " <<import << "\n";
     
        if (importFile.filename().string() == "iostream"||importFile.filename().string() == "<iostream>") {
            Ifiostream = true;
        }

    }
}
}
 
 std::string funcline; 
   while (std::getline(funcfile, funcline)) {
                    //提前判斷overwrite
          if(funcline.find("#overwrite")!= std::string::npos){enableoverwrite=true;}
        /*  if(funcline != std::string::npos){   size_t pos = funcline.find("@function<<");
            std::string funcname = funcline.substr(pos + 11); // "@function(" 長度 10
            funcname = funcname.substr(0, funcname.find(">>"));*/
            std::string funcname = runCommand(funcline,true);

 
            bool comment=isComment(funcline);
             if (comment) {continue; }
            if (funcname.empty()) continue;
             // if(shouldInjectFuction)  outfile <<funcname+"\n";
    
    }


    //預處理
    for(int i=0;i<tokenstream.size();i++) {find_slot(tokenstream[i]);}
          
     for(int i=0;i<tokenstream.size();i++) {
        outfile << runTokenFunc(tokenstream[i]);}
    
     /*
    for(auto& p:tokenstream){
       printToken(p);
    }*/

    if(enableoverwrite||gen_header) outfile << "/*";
        outfile << "int main() {\n";



    std::string line; 
    std::string extraFlags=""; // 存 @command() 的內容
    while (std::getline(infile, line)) {
        std::string_view fdcommd(line); 
        commentInReg=false;
        commentInReg=isComment(line);
        std::string code = runCommand(line,false);
        if (!code.empty()) outfile << code;
        if (commentInReg) {continue; }
        if (fdcommd.find("@command(") != std::string::npos) {
            size_t start = fdcommd.find("\"");
            size_t end = fdcommd.rfind("\"");
            if (start != std::string::npos && end != std::string::npos && end > start)
                extraFlags += " " + line.substr(start + 1, end - start - 1);
        }
        if(fdcommd.find("#useclang")!= std::string::npos){enableclang=true;}
        if(fdcommd.find("#usegcc")!= std::string::npos){enableclang=false;}
        if(fdcommd.find("#overwrite")!= std::string::npos){enableoverwrite=true;}
        if(fdcommd.find("#skipcompile")!= std::string::npos){skipcompile=true;}
 
    }
    for(auto& p:tokenstream) { outfile << runToken(p);}
    

    outfile << "\nreturn 0;\n}\n";
     if(enableoverwrite||gen_header) outfile << "*/";
    outfile.close();
std::string local=(isMac || isLinux)? "./":"";
    fs::path exePath = cpsPath.parent_path() / (local+cpsPath.stem().string() );// .exe後綴 : + ".exe");

    // 讀 include.ini 和 lib.ini
    std::string includeFlags = parseIni("include.ini", "-I");
    std::string libFlags = parseIni("lib.ini", "-L");
    
#if defined(_WIN32) || defined(_WIN64) 
 fs::path BackupcppPath,BackupexePath;
if(!enableoverwrite&&!skipcompile&&!gen_header){
              fs::copy_file(cppPath,"cppsptmp.cpp",fs::copy_options::overwrite_existing);
               BackupcppPath=cppPath;cppPath=cppPath.parent_path() / "cppsptmp.cpp";
               BackupexePath=exePath;exePath=exePath.parent_path() / "cppsptmp.exe";
}
 #endif             

    std::string gppCommand = "g++ \"" + cppPath.string() + "\"" + " -o \"" + exePath.string() + "\" "
                             + extraFlags + " "
                             + includeFlags + " "
                             + libFlags;
    if(enableclang) gppCommand = "clang++ \"" + cppPath.string() + "\"" + " -o \"" + exePath.string() + "\" "
                             + extraFlags + " "
                             + includeFlags + " "
                             + libFlags;
    if(enableoverwrite) gppCommand = extraFlags + " " + includeFlags + " "  + libFlags;
     if( skipcompile||gen_header){
        gppCommand="";
     }else{
            std::cout << "Compiling: " << gppCommand << "\n";int ret;
             #if defined(_WIN32) || defined(_WIN64) 
              ret=_wsystem(to_wide(gppCommand).c_str());
              if(!enableoverwrite){ 
              fs::rename("cppsptmp.cpp",BackupcppPath.string());
              fs::rename("cppsptmp.exe",BackupexePath.string()+".exe");
              exePath=BackupexePath;
              }
              #else
              ret = system(gppCommand.c_str()); 
             #endif
     

    if (ret != 0) {
        std::cerr << "Compilation failed!\n";
        return 1;
    }
} 
   if(!enableoverwrite) std::cout << "Compilation succeeded! Executable: " << exePath.string() << "\n";
   if(enableoverwrite) std::cout << "Compilation succeeded!\n";
     if(!enableoverwrite&&!gen_header) {int runexe;
        #if defined(_WIN32) || defined(_WIN64) 
               runexe=_wsystem(to_wide("\""+exePath.string()+"\"").c_str());
              #else
              std::string tmp="\""+exePath.string()+"\"";
              runexe= system(tmp.c_str());
             #endif
        }
        
    return 0;
}
