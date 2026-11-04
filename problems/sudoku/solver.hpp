#pragma once
#include <array>
#include <map>
#include "common.hpp"
using Board = std::array<int, 81>;
Status sudoku(Board &, bool, std::size_t, Metrics &);
Board parseBoard(const std::string &);
extern const std::map<std::string, std::string> examples;
