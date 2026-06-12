
/*
 * CmdParser.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */


/****************************************************************
        CmdParser.h  parse command-line args
                        
****************************************************************/

#pragma once

#include <optional>
#include <string>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <vector>

struct CmdParser {

    struct CmdOptions {
        std::optional<std::string> src;
        std::optional<std::string> dst;
        std::optional<std::string> ir;
        std::optional<int> sampleRate;
    } opt;


    void printUsage(const char* progName) {
        std::cout
            << "\nUsage: " << progName << " [options]\n"
            << "    Options:\n"
            << "      -r, --ref <name>       reference file to load\n"
            << "      -s, --src <name>       source file to load\n"
            << "      -o, --out <name>       Impulse Response File to save\n"
            << "      -f, --sr <value>       Sample Rate to save the Impulse Response File\n\n"
            << "      -h, --help             print this help and exit\n";
    }

    static bool parseFloat(const char* str, float& out) {
        char* end = nullptr;
        out = std::strtof(str, &end);
        return end != str && *end == '\0';
    }

    static bool parseInt(const char* str, int& out) {
        char* end = nullptr;
        out = (int)std::strtod(str, &end);
        return end != str && *end == '\0';
    }

    bool parseCmdLine(int argc, char** argv) {
        //if (argc > 3) return false;
        int count = 0;
        for (int i = 1; i < argc; ++i) {
            const char* arg = argv[i];

            if (std::strcmp(arg, "-s") == 0 || std::strcmp(arg, "--src") == 0) {
                if (i + 1 >= argc) {
                    std::cerr << "Error: --src requires a string\n";
                    return false;
                }
                opt.src = argv[++i];
            } else if (std::strcmp(arg, "-r") == 0 || std::strcmp(arg, "--ref") == 0) {
                if (i + 1 >= argc) {
                    std::cerr << "Error: --ref requires a string\n";
                    return false;
                }
                opt.dst = argv[++i];
            } else if (std::strcmp(arg, "-o") == 0 || std::strcmp(arg, "--out") == 0) {
                if (i + 1 >= argc) {
                    std::cerr << "Error: --out requires a string\n";
                    return false;
                }
                opt.ir = argv[++i];
            } else if (std::strcmp(arg, "-f") == 0 || std::strcmp(arg, "--sr") == 0) {
                if (i + 1 >= argc) {
                    std::cerr << "Error: --sr requires a value\n";
                    return false;
                }
                int value;
                if (!parseInt(argv[++i], value)) {
                    std::cerr << "Error: invalid rate value\n";
                    return false;
                }
                opt.sampleRate = value;
            } else if (std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--help") == 0) {
                return false;
            }
            ++count;
        }
        return true;
    }

};
