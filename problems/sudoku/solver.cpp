#include "solver.hpp"
#include "validator.hpp"
vector<int> candidates(const Board& b, int pos) {
    if(pos<0 || pos>=81) throw invalid_argument("Invalid cell index");
    unsigned used=0;
    for(int k=0;k<9;++k) {
        const int values[]={b[pos/9*9+k],b[k*9+pos%9],b[(pos/27*3+k/3)*9+(pos%9/3*3)+k%3]};
        for(int value:values) {
            if(value<0 || value>9) throw invalid_argument("Invalid cell value");
            if(value) used |= 1u << value;
        }
    }
    vector<int> result;
    for(int v=1;v<=9;++v) if(!(used & (1u<<v))) result.push_back(v);
    return result;
}

namespace {
struct CompactState {
    array<unsigned char,81> cells{};
    array<unsigned short,9> rows{},cols{},boxes{};
    unsigned char depth=0;
};
int boxIndex(int pos) { return (pos/27)*3+(pos%9)/3; }
unsigned allowed(const CompactState& state,int pos) {
    return 0x3FEu & ~(state.rows[pos/9] | state.cols[pos%9] | state.boxes[boxIndex(pos)]);
}
void place(CompactState& state,int pos,int value) {
    unsigned bit=1u<<value;
    state.cells[pos]=static_cast<unsigned char>(value);
    state.rows[pos/9]|=bit;state.cols[pos%9]|=bit;state.boxes[boxIndex(pos)]|=bit;
}
void undo(CompactState& state,int pos,int value) {
    unsigned bit=1u<<value;
    state.cells[pos]=0;
    state.rows[pos/9]&=~bit;state.cols[pos%9]&=~bit;state.boxes[boxIndex(pos)]&=~bit;
}
}
Status sudoku(Board& b, bool bfs, size_t cap, Metrics& m) {
    m={};
    if(!cap) throw invalid_argument("Node limit must be positive");
    if(!sudokuValid(b,false)) throw invalid_argument("Sudoku clues contain conflicts");
    CompactState state;
    vector<int> empty;
    for(int i=0;i<81;++i) {
        if(b[i]) place(state,i,b[i]);
        else empty.push_back(i);
    }
    auto publish=[&](const CompactState& solution) {
        for(int i=0;i<81;++i) b[i]=solution.cells[i];
    };
    if(bfs) {
        queue<CompactState> frontier;
        frontier.push(state);m.peak=1;
        while(!frontier.empty()) {
            if(m.visited>=cap) return Status::limit;
            CompactState current=frontier.front();frontier.pop();++m.visited;
            if(current.depth==empty.size()) {publish(current);return Status::solved;}
            int pos=empty[current.depth];
            const unsigned mask=allowed(current,pos);
            // Same FIFO order, first empty cell and ascending values as baseline.
            for(int value=1;value<=9;++value) {
                if(!(mask&(1u<<value))) continue;
                if(frontier.size()>=cap) return Status::limit;
                CompactState child=current;
                place(child,pos,value);++child.depth;
                frontier.push(child);++m.generated;m.peak=max(m.peak,frontier.size());
            }
        }
        return Status::unsat;
    }
    function<Status(size_t)> dfs=[&](size_t depth) {
        if(m.visited>=cap) return Status::limit;
        ++m.visited;
        if(depth==empty.size()) return Status::solved;
        const int pos=empty[depth];const unsigned mask=allowed(state,pos);
        for(int value=1;value<=9;++value) {
            if(!(mask&(1u<<value))) continue;
            place(state,pos,value);++m.generated;
            Status result=dfs(depth+1);
            if(result==Status::solved) return result;
            undo(state,pos,value);++m.backtracks;
            if(result==Status::limit) return result;
        }
        return Status::unsat;
    };
    Status result=dfs(0);
    if(result==Status::solved) publish(state);
    return result;
}
Board parseBoard(const string& text) {
    if(text.size()!=81) throw invalid_argument("Sudoku needs exactly 81 digits (0 or . for blanks)");
    Board b{};
    for(int i=0;i<81;++i) {
        char c=text[i];
        if(c=='.') b[i]=0;
        else if(c>='0' && c<='9') b[i]=c-'0';
        else throw invalid_argument("Invalid Sudoku character");
    }
    return b;
}
extern const map<string,string> examples={
 {"easy","530070000600195000098000060800060003400803001700020006060000280000419005000080079"},
 {"medium","000260701680070090190004500820100040004602900050003028009300074040050036703018000"},
 {"hard","100007090030020008009600500005300900010080002600004000300000010040000007007000300"}
};


