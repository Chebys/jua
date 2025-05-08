#include "jua-internal.h"
#include <unordered_map>

typedef Jua_Val* (*uni_fn)(Jua_Val*);
typedef Jua_Val* (*bin_fn)(Jua_Val*, Jua_Val*);

struct UniOper;
struct BinOper;

std::unordered_map<string, UniOper> uniOpers;
std::unordered_map<string, BinOper> binOpers;

struct UniOper{
    string name;
    uni_fn fn;
    UniOper(string n, uni_fn f): name(n), fn(f){}
};
struct BinOper{
    string name;
    bin_fn fn;
    BinOper(string n, bin_fn f): name(n), fn(f){}
};

Jua_Val* __unm(Jua_Val* val){
    return val;
}



static bool init(){
    return false;
}
static bool _ = init();