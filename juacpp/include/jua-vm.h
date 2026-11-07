#pragma once
#include "unordered_set"
#include <vector>
#include <algorithm>
#include "jua-value.h"
namespace Jua {

    struct VM{
    std::unordered_map<std::string, Value*> modules;
    std::unordered_set<Value*> ownedValues; //宿主长期持有的值，作为 GC 根

    //全部指针成员都必须初始化：任何未初始化的成员被当作 GC 根时，
    //都会让标记阶段去解引用栈上的垃圾值（历史 bug，见 改进清单 附C #16）
    Scope* _G = nullptr;
    Object* classProto = nullptr;
    Object* StringProto = nullptr;
    Object* NumberProto = nullptr;
    Object* BooleanProto = nullptr;
    Object* FunctionProto = nullptr;
    Object* ObjectProto = nullptr;
    Object* ArrayProto = nullptr;
    Object* BufferProto = nullptr;
    Object* RangeProto = nullptr;
    Object* ErrorProto = nullptr;
    Object* TryResProto = nullptr;

    NativeFunction* obj_new = nullptr;
    NativeFunction* obj_hasOwn = nullptr;
    NativeFunction* obj_next = nullptr;

    // ---- 垃圾回收（非移动标记-清除，见 gc.cpp） ----
    std::unordered_set<Value*> allValues; //所有由本 VM 分配、受回收管理的值
    std::vector<Value*> gcRoots;          //求值期间临时根（调用帧、迭代器、参数表）
    std::vector<Value*> markStack;        //标记阶段的显式栈，避免深递归
    size_t gcThreshold = 100000;          //存活值数超过它则在下个安全点回收
    size_t collectedCount = 0;            //累计回收次数（诊断用）
    size_t freedCount = 0;                //累计回收的值个数（诊断用）
    bool gcPending = false;
    bool collecting = false;
    Value* traceParent = nullptr; //诊断用：当前正在遍历的值
    const char* rootTag = nullptr; //诊断用：当前正在标记的根

    VM();
    virtual ~VM(){
        //进程结束时统一释放，避免调试器报告泄漏
        for(auto v: allValues) delete v;
        allValues.clear();
    }
    void run(const std::string&);
    Value* eval(const std::string&); //不捕获错误
    void own(Value*); //宿主登记长期持有的值（GC 根）

    // GC 接口
    void registerValue(Value*);
    void markFrom(Value*);
    void safePoint(Value* scope, Value* retval);
    size_t collect(); //立即回收，返回本次回收的个数
    size_t liveCount() const { return allValues.size(); }

    protected:
    void initBuiltins();
    void makeGlobal(); //在构造函数中调用，重写没有意义；要添加内置值请在子类构造函数中进行
    virtual std::string findModule(const std::string& name) = 0;
    virtual void j_stdout(const jualist&){};
    virtual void j_stderr(JuaError*){};
    NativeFunction* makeFunc(NativeFunction::Native fn){
        return new NativeFunction(this, fn);
    }
    //捕获 jua 值时必须用这个入口，否则 GC 看不到捕获值
    NativeFunction* makeFunc(NativeFunction::Native fn, std::vector<Value*> captures){
        return new NativeFunction(this, fn, std::move(captures));
    }
    Object* buildClass(NativeFunction::Native constructor);
    Object* makeRangeProto();
    Object* makeNumberProto();
    Object* makeStringProto();
    Object* makeFunctionProto();
    Object* makeObjectProto();

    Object* makeArrayProto();
    Object* makeBufferProto();
    Object* makeErrorProto();
    Object* makeTryResProto();

    Object* makeMath();
    Object* makeJSON();

    private:
    size_t idcounter = 0;
    Value* require(const std::string& name);
    typedef std::string Encoder(double);
    typedef double Decoder(const std::string&);
    NativeFunction* makeEncodeFunc(Encoder);
    NativeFunction* makeDecodeFunc(Decoder);
};

// 根集合的 RAII 守卫：把 gcRoots 恢复到构造时的长度，异常安全
struct RootGuard{
    VM* vm;
    size_t base;
    RootGuard(VM* v): vm(v), base(v->gcRoots.size()){}
    ~RootGuard(){ vm->gcRoots.resize(base); }
    RootGuard(const RootGuard&) = delete;
    RootGuard& operator=(const RootGuard&) = delete;
};

}
