#pragma once
#include "common.hpp"
using Board = array<int,81>;
vector<int> candidates(const Board&, int);
Status sudoku(Board&, bool, size_t, Metrics&);
Board parseBoard(const string&);
extern const map<string,string> examples;
