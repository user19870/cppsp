# cppsp
cppsp是一個轉譯式語言(原始碼轉原始碼)，可以把.cppsp檔案轉成.cpp並編譯、執行，是單人開發的實驗性語言，不適合大專案使用。此cppsp與c++ server pages無關。
* [English](https://github.com/user19870/cppsp)
* [中文](README_tw.md)
## 安裝
* 準備c++編譯器(gcc/clang)並設定到環境變數，讓cppsp可以呼叫g++、clang++指令
* 下載cppsp_compiler.exe或linux、mac版本或自己編譯
* 可以(不強制)把cppsp_compiler所在資料夾設定到環境變數
* 下載linux/mac版本時記得刪除後面的_linux.delete_linux、_mac.delete_mac
* (可選)修改cppsp_compiler.exe(或cppsp_compiler)名稱成任意名稱來更改編譯指令，例如改成cppsp、abcdef....
* 使用curl指令下載:
#### Windows:
``` 
curl -L -o cppsp_compiler.exe https://github.com/user19870/cppsp/raw/refs/heads/First/cppsp_compiler.exe
```
#### Linux:
```
  curl -L -o cppsp_compiler https://github.com/user19870/cppsp/raw/refs/heads/First/cppsp_compiler_linux.delete_linux
```
#### Mac:
```
  curl -L -o cppsp_compiler https://github.com/user19870/cppsp/raw/refs/heads/First/cppsp_compiler_mac.delete_mac
```
# 使用方法
* 使用cmd、控制台編譯.cppsp檔
```
cppsp_compiler script.cppsp( 如果沒設定到環境改成.\cppsp_compiler.exe or c:\...\cppsp_compiler.exe)
```
* 使用include.ini、lib.ini設定c++的include/lib資料夾
 ```
 include.ini:C:\...\include1,c:\...\include2
lib.ini:C:\...\lib1,c:\...\lib2
 ```
## 特性
* 只有print("hello world")也能編譯
* 可以只用所有c++標頭
* 可以用 @inject and @function 注入c++程式碼
## 關鍵字
* `#useclang` 或 `#usegcc` : 使用clang/gcc編譯
* `@command("...")`: 把參數加入編譯指令:-Os、-m64
* `#overwrite`:讓 `@command()` 覆蓋整個編譯指令: `@command("g++ -Os -m64 -nostdlib  -shared   -o dll.dll dll.cpp")` 並在 int main(){..}後自動加 */，需要手動 `＠funcion<</\*>>` 把main()註解掉，且原來生成在main()的print()等也會被註解
* `#skipcompile` :跳過編譯直接執行結果
* `import` :使用c++標頭(不需要<>、"") `import iostream,cstdio,x,y,.....`
* `@funcuion<<...>>`:  把原生c++程式碼注入到#include 後面 int main()前面
   `@inject(...)` : 把程式碼注入到int main(){...}
* `print()`: 把內容輸出到終端/控制台 `print("12\n"," ",1," ",2.1,true,false," ")`
* `input()`:  輸入變數但必須配合 `@inject()` 宣告變數
* `//`:註解
### 注意事項 ⚠️
* v1.2前關鍵字強制靠左沒有空格! 
* v1.3前關鍵字的()、<< >>不能跨行!
* `＠command()` 將不會支持跨行
```
＠command("-f1 -f2 ..... -f5") 
＠command("-f6 -f7 ....-f10") 
```
配合 `＃overwrite` 關鍵字
```
＠command("g++ -Os -m64 -nostdlib  -shared ") 
＠command(" -o dll.dll dll.cpp") 
 ```
## 範例
```cpp
 print("hello world")
```
* 測試用範例:
```cpp
@command("-mtune=native   -fomit-frame-pointer -static-libgcc   -ffunction-sections -fdata-sections -Wl,--gc-sections  -Wl,--as-needed  -s  -Wl,--strip-all  -Os -m64")
import iostream,vector
@function<<using namespace std;>>
print("12\n"," ",1," ",2.1,true,false," ")
print( "abc")
print(1,"\n") //abv
//print(1.1)
@inject(int x=1;int y=2;int z=3; auto is_bool = [](const std::string& s){ return s == "true" || s == "false";};)
input(x,y,z)
@function<<class cls{vector< string> cars = {"Volvo", "BMW", "Ford", "Mazda"};};>>
print(x+y+z)
```
* 簡易dll(非真正dll寫法，僅示意)
```cpp
#overwrite
@command("g++ -Os -m64 -nostdlib  -shared   -o dll.dll dll.cpp")
@function<<extern "C" __declspec(dllexport) int add(int a, int b) { return a * b;}>>
@function<</*>>
```
