# Simple-Shell
Long-Assignment for CMPT 201

This project implements a simple shell that provides command line prompt.
Adding an ampersand (`&`) at the end of the commamd results running the command in the background


## Table of Contents
1. [Navigation Guide](#1-navigation-guide)
2. [Installation](#install)
3. [Getting Started](#start)
4. [Features](#features)



<a name="navigation"></a>

## 1. Navigation Guide

```bash
repository
├── CMakeLists.txt
├── include/*.h     ## header files
├── gtest/*         ## test cases
├── src/*.c         ## implementations
```
`gtest/` contains the test cases for grading. We use [Google Test](https://github.com/google/googletest/tree/main) to develop and run the test cases.



<a name="install"></a>

## 2. Installation

This project uses clang/clang++ for CMake compilation. 

Make sure the following lines are in `~/.zshenv`:
```bash
  $ export CC=$(which clang)
  $ export CXX=$(which clang++)
```


<a name="start"></a>

## 3. Getting Started

Follow the instructions to run the main script _main.py_:

#### 3.1. Clone and Navigate
```bash
# 1. Clone this repo to your local machine
git clone $THISREPO
# 2. Navigate into the repository directory
cd $THISREPO
```

#### 3.2. generate executable 
```bash
# 1. Configuration (make sure no build/ directory exist)
cmake -S . -B build
# 2. Compilation
cmake --build build
```
This generates a executable file _shell_ in the build directory

#### 3.3. Run shell
```bash
# option A: test cases
cd build/
./shelltest

# option B: manual starting
./shell
```



<a name="features"></a>

## 4. Features
This simple shell supports basic Linux commands. Type `help` in the prompt to see more.
Note:
- This simple shell doesn't support either `>` (output redirection) or `|` (pipe) well.
- `!n` command runs command number n listed in history.
- `!!` command runs the last command entered.



## Acknowledgment

Created by Mohamed Hefeeda, modified by Brian Fraser, Keval Vora, Tianzheng Wang, and Steve
