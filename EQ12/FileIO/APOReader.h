
/*
 * APOReader.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "Band.h"

/****************************************************************
        APOReader - load a APO based EQ config reference file
****************************************************************/


class APOReader {
private:
    static constexpr int kNumBands = 12;

    struct CutFilter {
        bool enabled = false;
        double freqHz = 0.0;
        bool present = false;
    };

    struct ParsedFilter {
        bool enabled = false;
        Band::Type type = Band::Peak;
        double freqHz = 0.0;
        double gainDb = 0.0;
        double q = 0.707;
        int lineNo = 0;
    };

    static const char* bandTypeName(Band::Type t) {
        switch (t) {
            case Band::LowShelf:  return "Low Shelf";
            case Band::Peak:      return "Peak";
            case Band::HighShelf: return "High Shelf";
        }
        return "?";
    }

    static const char* typeToApoString(Band::Type t) {
        switch (t) {
            case Band::LowShelf:  return "LSC";
            case Band::Peak:      return "PK";
            case Band::HighShelf: return "HSC";
        }
        return "PK";
    }

public:
    void reset() {
        // set bands to default values and switch all bands off.
        for (int i = 0; i < 12; i++) {
            bands_[i] = Band{0, defs[i].type, defs[i].freqDef , 0.0, defs[i].qDef, 0};
        }
        bandAssigned_.fill(false);
        lowCut_ = CutFilter{};
        highCut_ = CutFilter{};
        preampDb_ = 0.0;
        havePreamp_ = false;
    }

    bool loadFile(const std::string& path) {
        reset();

        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Error: could not open file: " << path << "\n";
            return false;
        }

        static const std::regex filterRegex(
            R"(^\s*Filter\s+(\d+)\s*:\s*(ON|OFF)\s+(\S+)\s+Fc\s+([-+]?[0-9]*[.,]?[0-9]+)\s*Hz)"
            R"((?:\s+Gain\s+([-+]?[0-9]*[.,]?[0-9]+)\s*dB)?)"
            R"((?:\s+Q\s+([-+]?[0-9]*[.,]?[0-9]+))?)",
            std::regex::icase);

        static const std::regex preampRegex(
            R"(^\s*Preamp\s*:\s*([-+]?[0-9]*[.,]?[0-9]+)\s*dB)", std::regex::icase);

        std::vector<ParsedFilter> parametric;  // LSC/LS, PK, HSC/HS candidates

        std::string line;
        int lineNo = 0;
        while (std::getline(file, line)) {
            ++lineNo;
            std::string trimmed = trim(line);
            if (trimmed.empty() || trimmed[0] == '#') continue;

            std::smatch m;
            if (std::regex_search(trimmed, m, preampRegex)) {
                preampDb_ = parseLocaleDouble(m[1].str());
                havePreamp_ = true;
                continue;
            }

            if (!std::regex_search(trimmed, m, filterRegex)) continue;

            bool enabled = (toUpper(m[2].str()) == "ON");
            std::string typeStr = toUpper(m[3].str());
            double freq = parseLocaleDouble(m[4].str());
            bool hasGain = m[5].matched;
            double gain = hasGain ? parseLocaleDouble(m[5].str()) : 0.0;
            bool hasQ = m[6].matched;
            double q = hasQ ? parseLocaleDouble(m[6].str()) : 0.707;

            if (typeStr == "LSC" || typeStr == "LS" ||
                typeStr == "PK" ||
                typeStr == "HSC" || typeStr == "HS") {
                ParsedFilter pf;
                pf.enabled = enabled;
                pf.type = (typeStr == "LSC" || typeStr == "LS")  ? Band::LowShelf
                          : (typeStr == "HSC" || typeStr == "HS") ? Band::HighShelf
                                                                   : Band::Peak;
                pf.freqHz = freq;
                pf.gainDb = gain;
                pf.q = q;
                pf.lineNo = lineNo;
                parametric.push_back(pf);
            } else if (typeStr == "HP") {
                if (lowCut_.present) {
                   // debug output only
                   // std::cerr << "Warning (line " << lineNo
                   //           << "): duplicate HP (Low Cut) filter, keeping first one.\n";
                    continue;
                }
                lowCut_.enabled = enabled;
                lowCut_.freqHz = freq;
                lowCut_.present = true;
            } else if (typeStr == "LP") {
                if (highCut_.present) {
                    // debug output only
                   // std::cerr << "Warning (line " << lineNo
                   //           << "): duplicate LP (High Cut) filter, keeping first one.\n";
                    continue;
                }
                highCut_.enabled = enabled;
                highCut_.freqHz = freq;
                highCut_.present = true;
            } else {
              // debug output only
              //  std::cerr << "Warning (line " << lineNo << "): filter type '" << typeStr
              //            << "' is not supported by ToneShiftEQ12's biquad engine "
              //               "(only LSC/LS/PK/HSC/HS bands + HP/LP cut filters), skipping.\n";
            }
        }

        // Assign parsed filters to band slots, lowest frequency first, so
        // low-frequency bands aren't stolen by a later, higher-frequency
        // filter that merely appeared earlier in the file.
        std::sort(parametric.begin(), parametric.end(),
                  [](const ParsedFilter& a, const ParsedFilter& b) {
                      return a.freqHz < b.freqHz;
                  });

        for (const ParsedFilter& pf : parametric) {
            int slot = bestSlotForFreq(pf.freqHz, bandAssigned_);
            if (slot == -1) {
              // debug output only
              //  std::cerr << "Warning (line " << pf.lineNo
              //            << "): all " << kNumBands
              //            << " bands are already assigned, ignoring filter @ "
              //            << pf.freqHz << " Hz.\n";
                continue;
            }
            if (!(pf.freqHz >= defs[slot].freqMin && pf.freqHz <= defs[slot].freqMax)) {
              // debug output only
              //  std::cerr << "Note (line " << pf.lineNo << "): " << pf.freqHz
              //            << " Hz is outside every free band's range, using nearest "
              //               "free band "
              //            << (slot + 1) << " (" << defs[slot].freqMin << "-"
              //            << defs[slot].freqMax << " Hz).\n";
            }

            Band& b = bands_[slot];
            b.enabled = pf.enabled ? 1 : 0;
            b.type = pf.type;
            b.freq = pf.freqHz;
            b.gain = pf.gainDb;
            b.Q = pf.q;
            b.mute = 0;
            bandAssigned_[slot] = true;
        }

        return true;
    }

    const std::array<Band, kNumBands>& bands() const { return bands_; }
    const CutFilter& lowCut() const { return lowCut_; }
    const CutFilter& highCut() const { return highCut_; }
    const double preampDb() const {
        if (havePreamp_) return preampDb_;
        return 0.0;
    }
    bool slotWasAssigned(int i) const { return bandAssigned_[i]; }

    // Writes an Equalizer APO compatible config.txt from explicit values.
    bool saveFile(const std::string& path,
                  const std::array<Band, kNumBands>& bandsOut,
                  bool lowCutEnabled, double lowCutFreq,
                  bool highCutEnabled, double highCutFreq,
                  double preampDbOut) const {
        std::ofstream file(path);
        if (!file.is_open()) {
            std::cerr << "Error: could not open file for writing: " << path << "\n";
            return false;
        }

        file << "# Generated by ToneShiftEQ12\n";
        file << "Preamp: " << std::fixed << std::setprecision(2) << preampDbOut << " dB\n";

        for (int i = 0; i < kNumBands; ++i) {
            const Band& b = bandsOut[i];
            bool on = b.enabled && !b.mute;
            file << "Filter " << (i + 1) << ": " << (on ? "ON" : "OFF") << " "
                 << typeToApoString(b.type) << " Fc "
                 << std::fixed << std::setprecision(2) << b.freq << " Hz Gain "
                 << std::fixed << std::setprecision(2) << b.gain << " dB Q "
                 << std::fixed << std::setprecision(3) << b.Q << "\n";
        }

        file << "Filter " << (kNumBands + 1) << ": " << (lowCutEnabled ? "ON" : "OFF")
             << " HP Fc " << std::fixed << std::setprecision(2) << lowCutFreq << " Hz\n";
        file << "Filter " << (kNumBands + 2) << ": " << (highCutEnabled ? "ON" : "OFF")
             << " LP Fc " << std::fixed << std::setprecision(2) << highCutFreq << " Hz\n";

        return file.good();
    }
    // overload: derives Low Cut / High Cut "enabled" from the
    // frequency values themselves
    bool saveFile(const std::string& path,
                  const std::array<Band, kNumBands>& bandsOut,
                  double lowCutFreq, double highCutFreq,
                  double preampDbOut) const {
        constexpr double kLowCutMin  = 20.0;
        constexpr double kHighCutMax = 22000.0;
        constexpr double kMargin     = 0.02;  // 2%

        bool lowCutEnabled  = lowCutFreq  > kLowCutMin  * (1.0 + kMargin);
        bool highCutEnabled = highCutFreq < kHighCutMax * (1.0 - kMargin);

        return saveFile(path, bandsOut,
                         lowCutEnabled, lowCutFreq,
                         highCutEnabled, highCutFreq,
                         preampDbOut);
    }

    // overload: re-exports whatever this instance currently
    // holds from the last loadFile() call (no live GUI edits applied).
    bool saveFile(const std::string& path) const {
        return saveFile(path, bands_,
                         lowCut_.enabled, lowCut_.freqHz,
                         highCut_.enabled, highCut_.freqHz,
                         preampDb());
    }

private:
    std::array<Band, kNumBands> bands_{};
    std::array<bool, kNumBands> bandAssigned_{};
    CutFilter lowCut_;
    CutFilter highCut_;
    double preampDb_ = 0.0;
    bool havePreamp_ = false;

    static std::string toUpper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                        [](unsigned char c) { return std::toupper(c); });
        return s;
    }

    static std::string trim(const std::string& s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return "";
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    static double parseLocaleDouble(const std::string& raw) {
        std::string s = raw;
        std::replace(s.begin(), s.end(), ',', '.');
        return std::stod(s);
    }

    // Picks the best still-free band slot (index into defs[]/bands array) for
    // a given target frequency. Returns -1 if all slots are taken.
    static int bestSlotForFreq(double freq, const std::array<bool, kNumBands>& used) {
        int inRangeBest = -1;
        double inRangeBestDist = std::numeric_limits<double>::max();
        int fallbackBest = -1;
        double fallbackBestDist = std::numeric_limits<double>::max();

        for (int i = 0; i < kNumBands; ++i) {
            if (used[i]) continue;
            const BandDef& d = defs[i];
            if (freq >= d.freqMin && freq <= d.freqMax) {
                double dist = std::abs(freq - d.freqDef);
                if (dist < inRangeBestDist) {
                    inRangeBestDist = dist;
                    inRangeBest = i;
                }
            } else {
                double dist = std::min<double>(std::abs(freq - d.freqMin), std::abs(freq - d.freqMax));
                if (dist < fallbackBestDist) {
                    fallbackBestDist = dist;
                    fallbackBest = i;
                }
            }
        }
        return inRangeBest != -1 ? inRangeBest : fallbackBest;
    }

};
