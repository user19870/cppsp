# cppsp<img src="./res/cppsp.png" width="5%" alt="cppsp logo"/>

cppsp -- a transpiled language based on C++

## Install

Build from source using CMake or download a release. Keep it somewhere and optionally add it to your environment variables.

## Usage

- Use a terminal to build .cppsp file:

`cppsp_compiler script.cppsp`

- Setting c++ include/lib folder by .ini file
include.ini:C:\...\include1,c:\...\include2
lib.ini:C:\...\lib1,c:\...\lib2

## Features

- Can compile with only `print("hello world")` in .cppsp
- Can use almost any C++ header by import
- Can use C++ code by @inject and @function
- Enable indentation and multi-line after v1.3

## Keywords

- `#useclang` or `#usegcc` : use `clang++` or `g++` compile command
- `@command("...")`: add command when compile like: `-Os、-m64`
- #overwrite:make @command() overwrite g++ .... or clang++ compile command like @command("g++ -Os -m64 -nostdlib  -shared   -o dll.dll dll.cpp") and add "*/" in the end of int main{..} but you'll need ＠funcion<</\*>> to make comment work
- `import`: import header in c++ and accept import x,y,.....
- @funcuion<<...>>: inject everything(void()、int()、bool()、even #define and using namespace) in <<...>> to the space under #include above int main()
- `@inject(...)`: inject everything in (...) to int main{...}
- print(): print content to console like print("12\n"," ",1," ",2.1,true,false," ")
- `input()`: input data to variables, but need `@inject()` to declare varibles
- `//`:comment

### Warning ⚠️

- Cannot accept any space/blank before keyword before v1.2! 
- No multi-line before v1.3!
- `@command()` will never be multi-line but you can use following as an alternative:

```
＠command("-f1 -f2 ..... -f5") 
＠command("-f6 -f7 ....-f10") 
```

under ＃overwritender ＃overwrite

```
＠command("g++ -Os -m64 -nostdlib  -shared ") 
＠command(" -o dll.dll dll.cpp") 
```

## Examples

```
 print("hello world")
```

```
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
```
#overwrite
@command("g++ -Os -m64 -nostdlib  -shared   -o dll.dll dll.cpp")
@function<<extern "C" __declspec(dllexport) int add(int a, int b) { return a * b;}>>
@function<</*>>
```
