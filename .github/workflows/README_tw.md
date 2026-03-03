# cppsp
cppsp是一個轉譯式語言(原始碼轉原始碼)，可以把.cppsp檔案轉成.cpp並編譯、執行，是單人開發的實驗性語言，不適合大專案使用。此cppsp與c++ server pages無關。
* [English](https://github.com/user19870/cppsp)
* [中文](README_tw.md)
## 安裝
* 準備c++編譯器(gcc/clang)並設定到環境變數，讓cppsp可以呼叫g++、clang++指令
* [到官網](https://github.com/user19870/cppsp)下載cppsp_compiler.exe或linux、mac版本或自己編譯
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
cppsp_compiler mod.cppsp -header (產生.h檔案並註解掉int main....)
```
* 使用include.ini、lib.ini設定c++的include/lib資料夾
 ```
 include.ini:C:\...\include1,c:\...\include2
lib.ini:C:\...\lib1,c:\...\lib2
module.ini:C:...\modfolder1,c:...\modfolder2
 ```
## 特性
* 只有print("hello world")也能編譯
* 可以透過import使用所有c++標頭
- 可以透過import使用.cppsp模組
   - deepermod.cppsp 也能透過import使用c++標頭跟.cppsp模組 
* 可以用 @inject and @function 注入c++程式碼
* 使用var...type宣告
* 能在關鍵字和全域控制變數
## 關鍵字
* `#useclang` 或 `#usegcc` : 使用clang/gcc編譯
* `@command("...")`: 把參數加入編譯指令:-Os、-m64
* `#overwrite`:讓 `@command()` 覆蓋整個編譯指令: `@command("g++ -Os -m64 -nostdlib  -shared   -o dll.dll dll.cpp")` 並在 int main(){..}後分別自動加/*和*/
* `#skipcompile` :跳過編譯直接執行結果
- `import` :使用c++標頭(不需要<>、"") `import iostream,cstdio,x,y,.....`
    - 也能使用.cppsp 模組如`import a.b.mod`，a.b.mod代表路徑a/b/mod.cppsp，會從module.ini裡面設定的路徑尋找，a.b.mod會產生namespace a{ namespace b{ namespace mod{...}}}
* `package` :寫在.cppsp模組當中，用來替換import a.b.c產生的命名空間
* `@function<<...>>`:  把原生c++程式碼注入到#include 後面 int main()前面
   `@inject(...)` : 把程式碼注入到int main(){...}
* `print()`: 把內容輸出到終端/控制台 `print("12\n"," ",1," ",2.1,true,false," ")`
* `input()`:  輸入變數如 `input(a,b,c)`
* `var` ....`type` 可以宣告變數，允許多變數宣告，且宣告必須為值不能是算式，但可以用<{....}>把c++程式碼當成值來宣告，type只允許int/float/char/string/bool 
* `if/else/else if(...){...}` :跟c++的類似，但允許`if(input(x)>1)`，可以在{...}裡面寫變數操作(=,+,-,*,/,++....)和cppsp關鍵字
* `for(...){...}`: 支持`for( type i=0,i<10,i++)`、`for(type i=0,j=10;i<10&&j>0;i++,j++)`、`for(type i:x)`等多種方式，可以在{...}裡面寫變數操作(=,+,-,*,/,++....)和cppsp關鍵字
* `function f()...`:`function f() type {...return...}`可以定義有型別函數、 `function f(){...}`定義void函數、`function f()`宣告void函數、`function f(){...return...}`定義函數並使用c++的auto型別
```
function [std::pow,std::sort,abs,sqrt] // 註冊來自c++的函數但具有模板的函數依然需要<{...}>例如 std::sort(x,x+5,<{ std::greater<int>()}>)
```
* `struct S{...}` :定義一個結構體，結構體名稱會成為型別，所以可以使用var ... S之類的用法
* `@custom xxx("...",<{...}>,...)`: @custom可以讓使用者自訂語法，允許巢狀模板和單層命名空間隔離，"..."內容會變成產生的程式碼，<{...}>與其類似但它會被當成佔位符號，並在呼叫時被參數替換。 `namespace n{ @custom.... }`
```
@custom subs(<{T}>," sub(",<{T a}>,",",<{T b}>,")"," {return a-b;}")
subs(int ,int a,int b)
```
* `use`:使用命名空間例如 use a.b.c，@custom產生的語法也會被use影響
* `//`:註解
## 語法
* 一行控制一個變數或用逗號隔開
```
y=e()
y=4.6; y++ ;y++
```
* `<{...}> `:會把裡面的c++程式碼或所有東西當成cppsp關鍵字的元素例如:
```
 import math.h,iostream
print( <{pow(2,3)}>)
```
* `var`...`type`:
```
import  string,iostream
var a,c,d =  1,
<{(2*2+6)/2}>
,4 int
var b = "hello world" string
var f1,f2,f3 float
var c1 char
var b1 = <{1+1==2}> bool
input(f1)
print(a," ",c," ",d," ",b," ",b1," ",f1)
```
* 陣列和變數操作:
```
import iostream
var x={1,2,3} int
@inject("x[0]=3;x[1]=2; x[2]=1;")
if(true) {
  x[0]=4
}
for(int i=0,i<3,i++) {x[i]=0}
```
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
[更多範例](https://github.com/user19870/cppsp/tree/First/example)
