# Colibri-core

## About this Project

This project base on [Colibri Core](https://proycon.github.io/colibri-core/), which is a open source NLP library. 

>“Efficient n-gram & skipgram modelling on text corpora„

I added some new features to this library to make it more powerful.

- NER
- Bag of words
- Pattern compose [will be done in the next few weeks]

##  how to build
you need to first install

- cmake

- mingw64(windows)/gcc(linux)/clang(mac)


make sure all of above have been added to PATH.

Then, just open project.

In this project directory, type follow command to build project.

```cmake
cmake --build cmake_build_debug --target all -j 4
```

cd cmake_build_debug directory, you will find `*.exe`.

**This repo was developed in c++17, so make sure your compiler support at least c++ 17** 

## Entrance







