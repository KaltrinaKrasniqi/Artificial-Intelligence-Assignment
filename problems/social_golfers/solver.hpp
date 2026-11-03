#pragma once
#include "common.hpp"
using Schedule=vector<vector<vector<int>>>;
Status golfers(int,int,int,size_t,Schedule&,Metrics&,unsigned seed=1);
Status golfersLinearNine(size_t,Schedule&,Metrics&);
Status golfersTen(size_t,Schedule&,Metrics&);
