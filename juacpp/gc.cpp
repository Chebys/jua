// 垃圾回收：非移动、精确、stop-the-world 的标记-清除。
//
// 设计要点（详见 docs/adr/0001-垃圾回收.md）：
//  * 值的地址永不改变，因此 C++ 侧的裸 Value* 始终有效（只要它指向的值可达）。
//  * 只在“安全点”回收：Block::exec 的两条语句之间。此刻求值栈上活跃的
//    jua 值只有当前作用域与待返回值，二者都由 safePoint() 显式登记为根。
//  * 其余根：全局作用域、模块表、内置原型、宿主句柄、调用帧、迭代器。
#include "jua-vm.h"
#include <algorithm>

using namespace Jua;

void VM::registerValue(Value* v){
    allValues.insert(v);
    if(!gcPending && allValues.size() >= gcThreshold)
        gcPending = true;
}

void VM::markFrom(Value* v){
    if(!v || v->marked)return;
    v->marked = true;
    markStack.push_back(v);
}

size_t VM::collect(){
    if(collecting)return 0;
    collecting = true;
    gcPending = false;
    markStack.clear();

    // ---- 根 ----
    markFrom(_G);
    for(auto& kv: modules)markFrom(kv.second);
    markFrom(classProto);
    markFrom(StringProto);
    markFrom(NumberProto);
    markFrom(BooleanProto);
    markFrom(FunctionProto);
    markFrom(ObjectProto);
    markFrom(ArrayProto);
    markFrom(BufferProto);
    markFrom(RangeProto);
    markFrom(ErrorProto);
    markFrom(TryResProto);
    markFrom(obj_new);
    markFrom(obj_hasOwn);
    markFrom(obj_next);
    for(auto v: ownedValues)markFrom(v);
    for(auto v: gcRoots)markFrom(v);

    // ---- 标记（显式栈，避免深递归爆栈） ----
    while(!markStack.empty()){
        Value* v = markStack.back();
        markStack.pop_back();
        v->trace(this);
    }

    // ---- 清扫 ----
    size_t before = allValues.size();
    for(auto it = allValues.begin(); it != allValues.end(); ){
        Value* v = *it;
        if(v->marked){
            v->marked = false;
            ++it;
        }else{
            it = allValues.erase(it);
            delete v;
        }
    }
    size_t after = allValues.size();
    size_t freed = before - after;

    collectedCount++;
    freedCount += freed;
    // 阈值随存活量增长，避免每次回收后立刻又触发
    gcThreshold = std::max<size_t>(100000, after * 2);
    collecting = false;
    return freed;
}

void VM::safePoint(Value* scope, Value* retval){
    if(!gcPending || collecting)return;
    // 把当前活跃值临时登记为根，回收后再恢复
    size_t base = gcRoots.size();
    if(scope)gcRoots.push_back(scope);
    if(retval)gcRoots.push_back(retval);
    collect();
    gcRoots.resize(base);
}
