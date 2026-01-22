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
#include <functional>
bool isWindows=false;bool isMac=false;bool isLinux =false;
 #if defined(_WIN32) || defined(_WIN64) 
 #define isWindows 1 
 #elif defined(__APPLE__) && defined(__MACH__) 
 #define isMac 1 
 #elif defined(__linux__) 
 #define isLinux 1 
 #endif
//檢查dll依賴:objdump -p cppsp_compiler.exe | findstr ".dll" 
//備忘錄:設計if/else/for控制、設計變數系統、設計函數系統
namespace fs = std::filesystem;
bool Ifiostream=0;bool commentInReg=false;
// ====== 新增：萬用語法指令註冊器 ======
std::unordered_map<std::string, std::function<std::string(const std::string&)>> cpsCommands;
std::unordered_map<std::string, std::string> g_vars; // 全域變數 
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
    UNKNOWN     //13 未知

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
bool isTypeKeyword(const std::string& s) {
    static const std::unordered_set<std::string> typeKeywords = {
        "int", "float", "bool", "char", "string"
    };
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
    "&", "|", "^", "!", "<", ">",":"
};

std::vector<Token> tokenstream;

using tokencallback = std::function<std::string(const Token&)>;
std::unordered_map<std::string, tokencallback> tokenHandlers;
void registerToken(const std::string& name, tokencallback handler) {
    tokenHandlers[name] = handler;
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
            } else if (src[idx] == '\n') {
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
               src[idx] == '_' || src[idx] == '@' || src[idx] == '#')) {
            id += src[idx++];
        }

        TokenType ttype = TokenType::IDENTIFIER;
        if (isTypeKeyword(id))
           ttype = TokenType::TYPE;
        if (tokenHandlers.find(id) != tokenHandlers.end())
            ttype = TokenType::KEYWORD;            

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

        // 單行註解
        if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/') {
            std::string comment;
            while (i < src.size() && src[i] != '\n') {
                comment += src[i++];
            }
            root.children.push_back({TokenType::COMMENT, comment, lineno, {}});
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
                   
                
                bool ifstrend;if(src[i+1]==')'||src[i+1]==','||src[i+1]==' ') ifstrend=true; else ifstrend=false;
                   
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

        if (!root.children.empty() && root.children.back().type == TokenType::KEYWORD) { 
                if (!inner.empty()) {  //變成關鍵字子節點
               std::istringstream ss(inner);Token iner=tokenizeFile(ss,inner_lineno);
               for(auto& child : iner.children)  node.children.push_back(child); // 不再使用 reinterpret_cast
                         }
                  node.children.push_back({TokenType::END, std::string(1, endChar), lineno, {}});
                   // 把 BEGIN 作為前一個 KEYWORD 的 child
                 root.children.back().children.push_back(node);
}else if(startChar =='['&&!root.children.empty() && root.children.back().type == TokenType::IDENTIFIER){
 if (!inner.empty()){
    root.children.back().value += std::string(1,startChar)+inner+std::string(1, endChar);
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
   if (i + 1 < src.size() && src[i] == '<' && src[i + 1] == '{') {
    std::string startStr = "<{";
    std::string endStr = "}>";
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

   
    continue;
}
//var   變數系統
       int var_inner_lineno = lineno;
       if (src[i] == 'v' && src[i+1] == 'a' && src[i+2] == 'r' ) {
             
            Token node{TokenType::KEYWORD, "var", lineno, {}};Token invar;
            i += 3;bool varend=true;
  
            std::string inner;
            while (i < src.size()&&varend) {
                 std::istringstream ss(inner);
                    if (src[i] == '\n'){invar=tokenizeFile(ss,var_inner_lineno);
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
            
 
           
    root.children.push_back(node);
  


 
            continue;
        }



        // 運算子 + = > ...
     bool matched = false;
         for (const auto& op : operators) {
         if (i + op.size() <= src.size() &&
         src.compare(i, op.size(), op) == 0) {
        root.children.push_back({TokenType::OPERATOR, op, lineno, {}} );
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
            src[i] == '_' || src[i] == '@' || src[i] == '#') {
            root.children.push_back(parseIdentifierOrKeyword(i));
            continue;
        }

        if (src[i] == '\n') lineno++;
        i++; // 防止死循環
    }

    return root;
}



// ====== token 執行接口 ======
// runTokenFunc 對應原 funcfile while，執行 token handler
 
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
      if(node.type == TokenType::IDENTIFIER&&node.children.empty()){
        if(node.value=="true"||node.value=="false"){Token tfbool;tfbool.type=TokenType::STRING;
            result.type = TokenType::KEYWORD;tfbool.value="\""+node.value+"\""; return tfbool;}else{
        result.type = TokenType::IDENTIFIER; return node;}  }
        if(node.type == TokenType::KEYWORD&& !node.children.empty()&&!boolkey){ auto it = tokenHandlers.find(node.value);Token k;k.type=TokenType::UNKNOWN;if (it != tokenHandlers.end()){
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

void runTokenFunc(const Token& node, std::ofstream& outfile) {
   
}

// runToken 對應普通程式行
std::string runToken(const Token& node) {
    std::string result;
         auto it = tokenHandlers.find(node.value);
    if (it != tokenHandlers.end()) {
        return it->second(node);  // 傳入整個 KEYWORD 節點
    }


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
                    ||cont.type==TokenType::CHAR||cont.type==TokenType::INJECT||cont.type==TokenType::OPERATOR
                    ){ if( root.children[i+1].value=="}"||root.children[i+1].line_number==cont.line_number+1){ argcont+=";";}
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
                    ||cont.type==TokenType::CHAR||cont.type==TokenType::INJECT||cont.type==TokenType::OPERATOR
                    ){ if( root.children[i+1].value=="}"||root.children[i+1].line_number==cont.line_number+1){ argcont+=";";}
                        }   
                } }
        }
        return "else " + argcont + "\n";
    });
        registerToken("for", [](const Token& node) {
        std::string argcond,argcont,cur,cr,ed;
        for(auto& root: node.children){
            if(root.value=="("){ argcond="for(";
                for(auto& cond: root.children){
                   cur= singletoken(cond,";").value;
                    if(cur=="("||cur=="{"){for(auto& p:cond.children){cur+= singletoken(p,",").value;}}
                    if(cond.type==TokenType::IDENTIFIER){cur=" "+cur;}
                    if(cond.type==TokenType::TYPE){cur=(cur=="int")?"long long":(cur=="float")?"double":cur;}
                    argcond+=cur;
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
                    ||cont.type==TokenType::CHAR||cont.type==TokenType::INJECT||cont.type==TokenType::OPERATOR
                    ){ if( root.children[i+1].value=="}"||root.children[i+1].line_number==cont.line_number+1){ argcont+=";";}
                        }   
                } }
        }
        return argcond+argcont+"\n";
    });
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
   
int main(int argc, char* argv[]) {
    bool enableclang =false;bool skipcompile=false;bool enableoverwrite = false;
  
    //註冊
 
      registerToken("var", [](const Token& node){
             std::string arga,type,optrar,cur;int vat=1;std::vector<std::string> n,c; 
             type=node.children.back().value;
             for (auto& child : node.children){
                if(child.type==TokenType::OPERATOR){optrar=child.value;}
                if(child.type==TokenType::IDENTIFIER){ n.push_back(child.value);}
                else if(child.type==TokenType::STRING||child.type==TokenType::NUMBER||child.type==TokenType::CHAR||child.type==TokenType::INJECT){
                    c.push_back(child.value);}
                else if(child.value=="{"){cur="{";for(auto& p:child.children){cur+=p.value;} c.push_back(cur);cur="";n[c.size()-1]+="[]";}
                if(child.type==TokenType::SEPARATOR)vat++;     
             }
             for(size_t i=0;i<n.size();i++){
                if(optrar=="="||optrar==""){
                              if(n[i].empty())  break;
                 std::string val = (i < c.size()) ? c[i] : ""; 
                 std::string op = val.empty() ? "" : optrar;
                 if(type=="string"){arga+=(Ifiostream)?"std::string "+n[i]+op+val+";":"char "+n[i]+"[]"+op+val+";";}
                 else if(type=="int"){arga+="long long "+n[i]+op+val+";";}
                 else if(type=="float"){arga+="double "+n[i]+op+val+";";}
                 else if(type=="bool"){arga+="bool "+n[i]+op+val+";";}
                 else if(type=="char"){arga+="char "+n[i]+op+val+";";}
                }else{
                    if(n[i].empty())  break;
                    
                 std::string val = (i < c.size()) ? c[i] : "";
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
    std::string ifio=(Ifiostream)?";\n":");\n";
    const auto& root= node.children[0].children;//切換到'('或'{'後面的節點
     for (const auto& child : root) {
            curtoken= singletoken(child,""); cur=curtoken.value;
             if(curtoken.type==TokenType::NUMBER){ if(!opt){
                  if(stod(curtoken.value)==(int)stod(curtoken.value)){args+=(Ifiostream)?"std::cout<<"+cur:" printf(\"%d\","+cur;}
                  else args+= (Ifiostream)?"std::cout<<"+cur:" printf(\"%g\","+cur;}  else{args+=cur;} }
             if(curtoken.type==TokenType::SEPARATOR) {args+=ifio;opt=false;}     
             if(curtoken.value[0]=='L') {(Ifiostream)?args+="std::wcout<<"+cur :"wprintf("+cur;}else{
               if(curtoken.type==TokenType::STRING||curtoken.type==TokenType::CHAR){ args+=(Ifiostream)?"std::cout<<"+cur:"printf(" + cur;}}
             if(curtoken.type==TokenType::OPERATOR){  args += cur;opt=true;}
             if(curtoken.type==TokenType::INJECT){ args+=(Ifiostream)?"std::cout<<"+cur:"printf(" + cur;}
             if(curtoken.type==TokenType::IDENTIFIER){
                if(!opt){args+=(Ifiostream)?"std::cout<<"+cur:"printf("+cur;} else{args+=cur;}}
            if(child.type==TokenType::KEYWORD){
                         args+=singletoken(child,"").value;std::string varinside=child.children[0].children[0].value;
                         args+=(Ifiostream)?"std::cout<<"+varinside:"printf("+varinside;
                    }
                
               
    }
    return args+ifio;
});
registerToken("input",[](const Token& node){
    std::string args;
    const auto& root= node.children[0].children;//切換到'('或'{'後面的節點
      if(Ifiostream==false) args= "printf(\"need import iostream\")";
     for (const auto& child : root) {
        if(child.type==TokenType::IDENTIFIER) args+="std::cin>>"+child.value+";\n";
        if(child.type==TokenType::INJECT) args+="std::cin>>"+child.value+";\n";
    }
    return  args;
});
registerToken("@inject",[](const Token& node){
    int cont;  
    std::string args,cur; const auto& root= node.children[0].children; 
     for (auto& child : root) {
            cur= injecttoken(child).value; 
            if(child.type == TokenType::STRING) args += cur;
            std::cout<<args;
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
  fs::path cpsPath(argv[1]);
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
    std::ofstream outfile(cppPath);
    if (!outfile) {
        std::cerr << "Cannot create cpp file.\n";
        return 1;
    }

    std::ifstream tokenfile(cpsPath);
   Token root = tokenizeFile(tokenfile,1);
    tokenstream.push_back(root);


    outfile << "#include <stdio.h>\n";
    std::string importline; std::ifstream fileinclude(cpsPath);
while (std::getline(fileinclude, importline)) {
  bool comment=isComment(importline);std::string_view svimportline(importline);
if (!comment && svimportline.find("import ") != std::string::npos) {
    size_t pos = svimportline.find("import ");
    std::string imports = importline.substr(pos + 7); // "import " 長度 7
  
    imports = trim(imports);

    // 拆成多個標頭
    std::stringstream ss(imports);
    std::string header;
    while (std::getline(ss, header, ',')) {
        header = trim(header); // 每個標頭也要去掉空白
        if (header.empty()) continue;

        fs::path importFile = cpsPath.parent_path() / header;
        std::string import= importFile.lexically_relative(cpsPath.parent_path()).string();
    
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
                outfile <<funcname+"\n";
    
    }

    for(auto& p:tokenstream){
       printToken(p);
    }

    if(enableoverwrite) outfile << "/*";
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
    for(auto& p:tokenstream) {std::string tokcode=runToken(p);outfile << tokcode;}
    

    outfile << "\nreturn 0;\n}\n";
     if(enableoverwrite) outfile << "*/";
    outfile.close();
std::string local=(isMac || isLinux)? "./":"";
    fs::path exePath = cpsPath.parent_path() / (local+cpsPath.stem().string() );// .exe後綴 : + ".exe");

    // 讀 include.ini 和 lib.ini
    std::string includeFlags = parseIni("include.ini", "-I");
    std::string libFlags = parseIni("lib.ini", "-L");

    std::string gppCommand = "g++ \"" + cppPath.string() + "\" -o \"" + exePath.string() + "\" "
                             + extraFlags + " "
                             + includeFlags + " "
                             + libFlags;
    if(enableclang) gppCommand = "clang++ \"" + cppPath.string() + "\" -o \"" + exePath.string() + "\" "
                             + extraFlags + " "
                             + includeFlags + " "
                             + libFlags;
    if(enableoverwrite) gppCommand = extraFlags + " " + includeFlags + " "  + libFlags;
     if( skipcompile){
        gppCommand="";
     }else{
            std::cout << "Compiling: " << gppCommand << "\n";
             int ret = system(gppCommand.c_str());
     

    if (ret != 0) {
        std::cerr << "Compilation failed!\n";
        return 1;
    }
} 
   if(!enableoverwrite) std::cout << "Compilation succeeded! Executable: " << exePath.string() << "\n";
   if(enableoverwrite) std::cout << "Compilation succeeded!\n";
     if(!enableoverwrite) int runexe= system(exePath.string().c_str());
    return 0;
}
