MAQAO (Modular Assembly Quality Analyzer and Optimizer)


# Outline

[Pre requisites](#pre-requisites)   
[Installation](#installation)   
[Documentation](#documentation)   
[Running MAQAO tools](#running-maqao-tools)

# Pre-requisites
The following packages and softwares have to be installed before launching the installation:
- gcc
- g++
- cmake (>= 2.8.8, [CMake software](http://www.cmake.org/cmake/resources/software.html "CMake download page"))
- glibc-static
- libstdc++-static  
 
If you choose the original lua package (and not luajit) then you will also need:   
- libreadline-dev
- libncurses-dev

# Installation
MAQAO is a cmake based project.   
Go into the MAQAO folder   
If no "build" directory is present, create it
```bash
>$ mkdir build
```
Go into the build folder:
```bash
>$ cd build
```
Then run the following commands:
```bash
>$ cmake ..
>$ make
```

# Documentation
The documentation generation is optional and need several softwares:
- for the LUA API documentation, luadoc is needed ([LuaDoc software](http://keplerproject.github.com/luadoc/ "LuaDoc home page"))
- for the C API documentation, doxygen is needed ([Doxygen tool](http://www.stack.nl/~dimitri/doxygen/ "Doxygen home page"))

To build the documentation, go in the build directory and type:
```bash
>$ make doc
```
Generated files are available trough a web browser. The LUA API documentation is called   
DeveloperGuide.html and the C API documentation is called CoreDeveloperGuide.html. Both   
are located in MAQAO/doc

# Running MAQAO modules
A MAQAO module can be launched with the following command:
```bash
>$ maqao <module> [args]
```
For instance to list the functions in the binary /path_to/my_binary:
```bash
>$ maqao analyze --list-functions /path_to/my_binary
```


The -h option provides help for a given module.   


To execute user-defined MAQAO/Lua scripts, use:
```bash
>$ maqao <lua-script> [args]
```
Note that MAQAO scripts are Lua scripts that can use the MAQAO Lua API extensions (see documentation for more information)   


For general help on MAQAO:
```bash
>$ maqao -h
```

