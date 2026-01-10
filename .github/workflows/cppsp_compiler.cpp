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
//備忘錄: 未來做token儲存.cppsp內容，用unorder；vector各種方法做出token分類如何關鍵字、關鍵字的()裡面參數、變數、數字、字串等，只掃描一次.cppsp檔案
//之後直接讀取token
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
    ROOT,       // container 根節點，不算實際 token
    BEGIN,  // ( 、 <<
    END,     // ) 、 >>

    IDENTIFIER, // 變數名稱
    NUMBER,     // 數字
    STRING,     // 字串
    OPERATOR,   // 運算子+ - = += * /
    TYPE,       // 資料型態int float bool string
    SEPARATOR,  // 分隔符號: , ;

    KEYWORD,    // 關鍵字: print println input @inject @function
    COMMENT,    // 註解
    UNKNOWN     // 未知

};
struct Token {
    TokenType type;
    std::string value;
    size_t line_number;
    std::vector<Token> children; // 用於括號、{}、<<>> 等內部 token
};
struct TokenizeState {
    int braceDepth = 0;      // { }
    int parenDepth = 0;      // ( )
    int bracketDepth = 0;    // [ ]
    int chevronDepth = 0;    // << >>

    bool inString = false;
    bool inBlockComment = false;
    bool inFunction = false;
};

bool isTypeKeyword(const std::string& s) {
    static const std::unordered_set<std::string> typeKeywords = {
        "int", "double", "float", "bool", "char", "string", "void"
    };
    return typeKeywords.find(s) != typeKeywords.end();
}

std::vector<Token> tokenstream;

using tokencallback = std::function<std::string(const Token&)>;
std::unordered_map<std::string, tokencallback> tokenHandlers;
void registerToken(const std::string& name, tokencallback handler) {
    tokenHandlers[name] = handler;
}

 
Token tokenizeLine(const std::string& line, size_t lineno, TokenizeState& state) {
    Token root{TokenType::ROOT,"",lineno,{}};
    size_t i = 0;

    auto skipWhitespace = [&](size_t& idx){
        while(idx<line.size() && (line[idx]==' '||line[idx]=='\t')) idx++;
    };

    auto parseIdentifierOrKeyword = [&](size_t& idx) -> Token {
        std::string id;
        while(idx<line.size() && ((line[idx]>='a' && line[idx]<='z')||(line[idx]>='A' && line[idx]<='Z')||(line[idx]>='0' && line[idx]<='9')||line[idx]=='_'||line[idx]=='@'||line[idx]=='#')) {
            id += line[idx++];
        }
        TokenType ttype = TokenType::IDENTIFIER;
        if(tokenHandlers.find(id) != tokenHandlers.end()) ttype = TokenType::KEYWORD;
        if(isTypeKeyword(id)) ttype = TokenType::TYPE;
        return {ttype,id,lineno,{}};
    };

    auto parseNumber = [&](size_t& idx) -> Token {
        std::string num;
        while(idx<line.size() && ((line[idx]>='0' && line[idx]<='9')||line[idx]=='.')) num+=line[idx++];
        return {TokenType::NUMBER,num,lineno,{}};
    };

    while(i<line.size()) {
        skipWhitespace(i);
        if(i>=line.size()) break;

        // 單行註解
        if(line[i]=='/' && i+1<line.size() && line[i+1]=='/') {
            root.children.push_back({TokenType::COMMENT,line.substr(i),lineno,{}});
            break;
        }

        // 字串
        if(line[i]=='"') {
            std::string str="\""; i++; 
            while(i<line.size()) {
                if(line[i]=='\\' && i+1<line.size()){ str+=line[i]; str+=line[i+1]; i+=2; continue;}
                if(line[i]=='"'){ str+='"'; i++; break; }
              str+=line[i];i++;
             
            }
            root.children.push_back({TokenType::STRING,str,lineno,{}});
            continue;
        }

        // 括號/花括號
        if(line[i]=='(' || line[i]=='{' ) {
            Token node{TokenType::BEGIN,std::string(1,line[i]),lineno,{}};
            char startChar = line[i];
            char endChar = (startChar=='(')?')':'}';
            int depth=1;
            i++;
            std::string inner;
            while(i<line.size() && depth>0) {
                if(line[i]==startChar) depth++;
                if(line[i]==endChar) depth--;
                if(depth>0) inner+=line[i];
                i++;
            }
            if(!inner.empty()) node.children.push_back(tokenizeLine(inner,lineno,state));
            root.children.push_back(node);
            root.children.push_back({TokenType::END,std::string(1,endChar),lineno,{}});
            
            continue;
        }

        // << >> 
        if((line[i]=='<'&&i+1<line.size() && line[i+1]=='<') || (line[i]=='>'&&i+1<line.size() && line[i+1]=='>')) {
            std::string startStr = line.substr(i,2);
            std::string endStr = (startStr=="<<")?">>":"<<";
            Token node{TokenType::BEGIN,startStr,lineno,{}};
            int depth=1; i+=2;
            std::string inner;
            while(i+1<line.size() && depth>0) {
                if(line.substr(i,2)==startStr) depth++;
                if(line.substr(i,2)==endStr) depth--;
                if(depth>0) inner+=line[i];
                i++;
            }
            if(!inner.empty()) node.children.push_back(tokenizeLine(inner,lineno,state));
            root.children.push_back(node);
            root.children.push_back({TokenType::END,endStr,lineno,{}});
            continue;
        }

        // 運算子
        if(std::string("=+-*/%&|^!<>").find(line[i])!=std::string::npos) {
            root.children.push_back({TokenType::OPERATOR,std::string(1,line[i]),lineno,{}});
            i++;
            continue;
        }

        // 數字
        if(line[i]>='0' && line[i]<='9') {
            root.children.push_back(parseNumber(i));
            continue;
        }

        // 識別字 / keyword / type
        if((line[i]>='a' && line[i]<='z')||(line[i]>='A' && line[i]<='Z')||line[i]=='_'||line[i]=='@'||line[i]=='#') {
            root.children.push_back(parseIdentifierOrKeyword(i));
            continue;
        } 
        


        i++; // 防止死循環
    }

    return root;
}



// ====== token 執行接口 ======
// runTokenFunc 對應原 funcfile while，執行 token handler
void runTokenFunc(const Token& node, std::ofstream& outfile) {
   
}

// runToken 對應普通程式行
std::string runToken(const Token& node) {
       std::string result;

    for (size_t i = 0; i < node.children.size(); ++i) {
        const Token& cur = node.children[i];

        if (cur.type == TokenType::KEYWORD) {
            auto it = tokenHandlers.find(cur.value);
            if (it != tokenHandlers.end()) {

                // 看下一個是不是 BEGIN
                if (i + 1 < node.children.size() &&
                    node.children[i + 1].type == TokenType::BEGIN) {

                    result += it->second(node.children[i + 1]);
                    ++i; // 跳過 BEGIN
                } else {
                    result += it->second(cur);
                }
                continue;
            }
        }
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
    std::string pad(indent*2, ' ');
    if(node.type != TokenType::ROOT && !node.value.empty()){
    std::cout << pad << "Token(type=" << (int)node.type
              << ", value=\""<<escapeUtf8(node.value)
              << "\", line=" << node.line_number+1 << ")\n";
    }
    for(auto& child: node.children){
        printToken(child, indent+1);
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
//去空格
inline std::string noblank(const std::string& token) {
       // 去掉前後空格
    std::string v = token;
    auto trim = [](std::string& s){
        while (!s.empty() && isspace(s.front())) s.erase(s.begin());
        while (!s.empty() && isspace(s.back())) s.pop_back();
    };  trim(v);
    // 空字串直接跳過
    if (v.empty()) return "";
    // 布林判斷
    if (v == "true")  return "printf(\"true\");\n";  if (v == "false") return "printf(\"false\");\n";
  // 判斷數字
    char* end;
    double num = std::strtod(v.c_str(), &end);
    if (*end == '\0') {// 判斷整數或浮點
        if (num == (int)num) return "{ auto _t = (int)" + v + "; printf(\"%d\", _t); }\n";
        else return "{ auto _t = " + v + "; printf(\"%g\", _t); }\n";  }
 // 判斷字串（含 "abc"）或 fallback
    if (!v.empty() && v.front()=='"' && v.back()=='"')  return "printf(" + v + ");\n";
    // 其他情況也當字串
    return "printf(\"" + v + "\");\n";}
    
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
    registerCommand("print","(",")", [](const std::string& args) {
    std::stringstream ss(args);
    std::string tok;
    std::vector<std::string> v;
    while (std::getline(ss, tok, ',')) v.push_back(tok);
    std::string out;double iorf;
      //處理非" "的參數
      if(v.empty()) {for (size_t i =0; i < v.size(); ++i)v[i]= noblank(v[i]);}
    if (!v.empty()) { 
    for (size_t i =0; i < v.size(); ++i) {
    std::string cur = v[i];
    if ( cur == "true") { out += "printf(\"true\");\n";}
    else if (cur == "false") { out += "printf(\"false\");\n"; }
    else if (is_number(cur)) {
        double iorf = std::stod(cur);
        if (iorf == (int)iorf)   out += "{ int _t = " + cur + "; printf(\"%d\", _t); }\n";
       else out += "{ double _t = " + cur + "; printf(\"%g\", _t); }\n"; }
       else if(cur[0]== 'L') out +=(Ifiostream)? "std::wcout<<"+cur+";\n" :"wprintf("+cur+");\n";
       else if(Ifiostream==true) out +="std::cout<<"+cur+";\n";
       else {  out += "printf(" + cur + ");\n";}
}
    }

    return out;
});

registerToken("println", [](const Token& begin) {
    std::string args,out;

    for (const auto& c : begin.children) {
        args += c.value;
    }
    out ="printf(" + args + "); printf(\"\\n\");\n";
  if(args.empty()) out ="";
    return out;
}); 

registerCommand("input","(",")",  [](const std::string& args) {
        std::stringstream ss(args);
    std::string tok,out;
    std::vector<std::string> v;
    while (std::getline(ss, tok, ',')) v.push_back(tok);
    if(Ifiostream==false) out= "printf(\"need import iostream\")";
    if (!v.empty()) { 
    for(int i=0;i<v.size();i++){
        out +="std::cin>>"+v[i]+";\n";
    }}
    return out;
});

registerCommand("@inject","(",")",  [](const std::string& args) {
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
        outfile << "#include \"" << importFile.lexically_relative(cpsPath.parent_path()).string() << "\"\n";
     
        if (importFile.filename().string() == "iostream") {
            Ifiostream = true;
        }

    }
}
}

 std::string funcline;size_t line_no=0;
 TokenizeState state={0,0,0,0,false,false,true};
   while (std::getline(funcfile, funcline)) {
                    //提前判斷overwrite
          if(funcline.find("#overwrite")!= std::string::npos){enableoverwrite=true;}
        /*  if(funcline != std::string::npos){   size_t pos = funcline.find("@function<<");
            std::string funcname = funcline.substr(pos + 11); // "@function(" 長度 10
            funcname = funcname.substr(0, funcname.find(">>"));*/
            std::string funcname = runCommand(funcline,true);
          
            
            Token node= tokenizeLine(funcline,line_no, state);
            tokenstream.push_back(node);
            line_no++;

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
        for(auto& p:tokenstream){std::string code = runToken(p);outfile << code;}
        if(fdcommd.find("#useclang")!= std::string::npos){enableclang=true;}
        if(fdcommd.find("#usegcc")!= std::string::npos){enableclang=false;}
        if(fdcommd.find("#overwrite")!= std::string::npos){enableoverwrite=true;}
        if(fdcommd.find("#skipcompile")!= std::string::npos){skipcompile=true;}
    }

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
