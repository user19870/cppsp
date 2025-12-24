#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <string.h>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <unordered_map>
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
    size_t l = line.find(h);
    
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
    if (!inMultiLine && (line.find("//") == 0 || line.empty())) return ""; 
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
        size_t cmdPos = line.find(p.first); if (cmdPos == std::string::npos) continue;
        // 確保指令不是變數的一部分 (簡單邊界檢查，可選)
        // if (cmdPos > 0 && isalnum(line[cmdPos-1])) continue; 
        if (commentInReg) return ""; // 假設這是全域變數
        for (auto& note : func_head_end) {
            const std::string& hd = note.first; const std::string& ed = note.second;
            // 檢查這一行是否有對應的起始符號 (例如 "(")
            if (line.find(hd) == std::string::npos) continue;
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
    bool enableclang =false;
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

registerCommand("println","(",")",  [](const std::string& args) {
    return "printf(" + args + "); printf(\"\\n\");\n";
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
        return 1;
    }

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

    fs::path cppPath = cpsPath.parent_path() / (cpsPath.stem().string() + ".cpp");
    std::ofstream outfile(cppPath);
    if (!outfile) {
        std::cerr << "Cannot create cpp file.\n";
        return 1;
    }

    outfile << "#include <stdio.h>\n";
    std::string importline; std::ifstream fileinclude(cpsPath);
while (std::getline(fileinclude, importline)) {
  bool comment=isComment(importline);
if (!comment && importline.find("import ") != std::string::npos) {
    size_t pos = importline.find("import ");
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
 std::string funcline;
   while (std::getline(funcfile, funcline)) {
    
        /*  if(funcline != std::string::npos){   size_t pos = funcline.find("@function<<");
            std::string funcname = funcline.substr(pos + 11); // "@function(" 長度 10
            funcname = funcname.substr(0, funcname.find(">>"));*/
            std::string funcname = runCommand(funcline,true);
            bool comment=isComment(funcline);
             if (comment) {continue; }
            if (funcname.empty()) continue;
                outfile <<funcname+"\n";
    }
    bool enableoverwrite = false;
 if(enableoverwrite) outfile << "/*";
        outfile << "int main() {\n";
    std::string line;
    std::string extraFlags=""; // 存 @command() 的內容
    while (std::getline(infile, line)) {
        commentInReg=false;
        commentInReg=isComment(line);
        std::string code = runCommand(line,false);
        if (!code.empty()) outfile << code;
        if (commentInReg) {continue; }
        if (line.find("@command(") != std::string::npos) {
            size_t start = line.find("\"");
            size_t end = line.rfind("\"");
            if (start != std::string::npos && end != std::string::npos && end > start)
                extraFlags += " " + line.substr(start + 1, end - start - 1);
        }
        if(line.find("#useclang")!= std::string::npos){enableclang=true;}
        if(line.find("#usegcc")!= std::string::npos){enableclang=false;}
        if(line.find("#overwrite")!= std::string::npos){enableoverwrite=true;}
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

    std::cout << "Compiling: " << gppCommand << "\n";
    int ret = system(gppCommand.c_str());

    if (ret != 0) {
        std::cerr << "Compilation failed!\n";
        return 1;
    }

   if(!enableoverwrite) std::cout << "Compilation succeeded! Executable: " << exePath.string() << "\n";
   if(enableoverwrite) std::cout << "Compilation succeeded!\n";
     if(!enableoverwrite) int runexe= system(exePath.string().c_str());
    return 0;
}
