------
[Home](index.md) - 
[News](news.md) - 
[Documentation](documentation.md) - 
[Tutorials](tutorials.md) -
[Contacts](contacts.md) -
[Team](team.md) -
[LProf](lprof.md) -
[CQA](cqa.md) -
[OneView](oneview.md)

------

# Outline

[Tools suite](#Tools-suite)   
[Features](#features)   
[Documentation](#documentation)   

# Tools-suite

MAQAO is a suite of tools for profiling, analyzing and optimizing HPC applications. It provides a low-overhead profiler capable of handling any runtime, [LProf](##LProf); a quick, yet accurate, static analyzer, [CQA](##CQA), which allow to tune your code for the very specific characteristics of your micro-architecture; and a result aggregator module, [OneView](##OneView), presenting performance reports and pinpointing bottlenecks.

These tools are built on top of the MAQAO core capabilities:
* A binary manipulation layer that unlocks the possibility to evaluate the compilers' output for your architecture and provides binary instrumentation for precise profiling,
* A reconstruction process offering multiple high-level views (loops, functions) connected with origin in the source code,
* And a set of analyzes building a complete suite of useful representations (control flow, data dependencies, etc.) and delivering numerous metrics for specialized tools.

In order to make MAQAO a convenient tool for researchers, a C API is provided for anyone to build its own module and a scripting langage (Lua) has been embedded and extended to provide quick and easy access to any metric.

To help industrial partners, MAQAO has been designed to have a very limited list of dependencies, all of them are included in standard Linux server distributions. Moreover, static binary releases are provided to obtain a quick and worry-free installation.

## LProf

Tested up to one hundred thousands cores.

See the [LProf](lprof.md) dedicated page for more information.

## CQA

See the [CQA](cqa.md) dedicated page for more information.

## OneView

See the [OneView](oneview.md) dedicated page for more information.

# Features

## Architectures

Currently, MAQAO handles x86 32-bit and 64-bit programs

## Operating system

MAQAO has been designed to help the HPC community and as such is heavily focused on platform running scientific computations. Right now, it supports Linux applications but can be compiled on both Linux and Windows (only static analyses are operational).

# Documentation

