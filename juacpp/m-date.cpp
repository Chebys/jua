#include "jua-value.h"
#include "jua-vm.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>

using namespace Jua;
using std::string;

namespace {

constexpr double MS_PER_DAY = 86400000.0;
constexpr double MAX_TIME = 8.64e15;      // ±10^8 天
constexpr int64_t MAX_DAYS = 100000000;

int64_t floordiv(int64_t a, int64_t b){
    int64_t q = a / b;
    int64_t r = a % b;
    if(r != 0 && (r < 0) != (b < 0)) q--;
    return q;
}
int64_t floormod(int64_t a, int64_t b){
    return a - floordiv(a, b) * b;
}

// 公历日期与「1970-01-01 起的天数」互转（Howard Hinnant 的算法）
int64_t days_from_civil(int64_t y, int64_t m, int64_t d){
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
void civil_from_days(int64_t z, int64_t& y, int64_t& m, int64_t& d){
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const int64_t doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = yoe + era * 400;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp + (mp < 10 ? 3 : -9);
    y += (m <= 2);
}

struct Fields{
    int64_t y = 1970, m = 1, d = 1;
    int64_t h = 0, mi = 0, s = 0, ms = 0;
    int64_t wd = 4; // 1=周一 … 7=周日；1970-01-01 是周四
};

// 字段越界时进位（同 JS）。返回 NaN 表示超出可表示范围。
double make_time(int64_t y, int64_t mo, int64_t d,
                 int64_t h, int64_t mi, int64_t s, int64_t ms){
    int64_t zero = mo - 1;
    y += floordiv(zero, 12);
    mo = floormod(zero, 12) + 1;

    int64_t days = days_from_civil(y, mo, 1) + (d - 1);
    int64_t tod = ((h * 60 + mi) * 60 + s) * 1000 + ms;
    days += floordiv(tod, 86400000LL);
    tod = floormod(tod, 86400000LL);

    if(days > MAX_DAYS || days < -MAX_DAYS) return NAN;
    double t = (double)days * MS_PER_DAY + (double)tod;
    if(!(t >= -MAX_TIME && t <= MAX_TIME)) return NAN;
    return t;
}

bool fields_from_time(double t, Fields& f){
    if(std::isnan(t) || !(t >= -MAX_TIME && t <= MAX_TIME)) return false;
    int64_t ti = (int64_t)std::floor(t);
    int64_t days = floordiv(ti, 86400000LL);
    int64_t tod = ti - days * 86400000LL;
    civil_from_days(days, f.y, f.m, f.d);
    f.h = tod / 3600000; tod %= 3600000;
    f.mi = tod / 60000; tod %= 60000;
    f.s = tod / 1000;
    f.ms = tod % 1000;
    f.wd = floormod(days + 3, 7) + 1;
    return true;
}

double now_ms(){
    using namespace std::chrono;
    auto since = system_clock::now().time_since_epoch();
    return (double)duration_cast<milliseconds>(since).count();
}

string pad(int64_t v, int width){
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%0*lld", (int)width, (long long)v);
    return string(buf);
}

// 有效日期返回 ISO 8601 文本；无效日期由调用者另行处理
string iso_from_time(double t){
    Fields f;
    if(!fields_from_time(t, f)) return "Invalid Date";
    string year;
    if(f.y >= 0 && f.y <= 9999){
        year = pad(f.y, 4);
    }else{
        year = (f.y < 0 ? "-" : "+") + pad(f.y < 0 ? -f.y : f.y, 6);
    }
    return year + "-" + pad(f.m, 2) + "-" + pad(f.d, 2)
         + "T" + pad(f.h, 2) + ":" + pad(f.mi, 2) + ":" + pad(f.s, 2)
         + "." + pad(f.ms, 3) + "Z";
}

bool is_digit(char c){ return c >= '0' && c <= '9'; }

// 解析 ISO 8601 子集；失败返回 NaN
double parse_iso(const string& src){
    size_t pos = 0;
    auto read_digits = [&](int n, int64_t& out) -> bool {
        if(pos + (size_t)n > src.size()) return false;
        int64_t v = 0;
        for(int i=0;i<n;i++){
            if(!is_digit(src[pos + i])) return false;
            v = v * 10 + (src[pos + i] - '0');
        }
        pos += (size_t)n;
        out = v;
        return true;
    };
    auto expect = [&](char c) -> bool {
        if(pos >= src.size() || src[pos] != c) return false;
        pos++;
        return true;
    };

    int64_t y, mo, d, h = 0, mi = 0, s = 0, ms = 0;
    if(!read_digits(4, y)) return NAN;
    if(!expect('-')) return NAN;
    if(!read_digits(2, mo)) return NAN;
    if(!expect('-')) return NAN;
    if(!read_digits(2, d)) return NAN;
    if(mo < 1 || mo > 12 || d < 1 || d > 31) return NAN;

    if(pos < src.size() && src[pos] == 'T'){
        pos++;
        if(!read_digits(2, h)) return NAN;
        if(!expect(':')) return NAN;
        if(!read_digits(2, mi)) return NAN;
        if(h > 23 || mi > 59) return NAN;
        if(pos < src.size() && src[pos] == ':'){
            pos++;
            if(!read_digits(2, s)) return NAN;
            if(s > 59) return NAN;
            if(pos < src.size() && src[pos] == '.'){
                pos++;
                if(!read_digits(3, ms)) return NAN;
                if(ms > 999) return NAN;
            }
        }
    }

    int64_t offset_min = 0;
    if(pos < src.size()){
        char c = src[pos];
        if(c == 'Z'){
            pos++;
        }else if(c == '+' || c == '-'){
            int sign = (c == '-') ? -1 : 1;
            pos++;
            int64_t oh, om;
            if(!read_digits(2, oh)) return NAN;
            if(!expect(':')) return NAN;
            if(!read_digits(2, om)) return NAN;
            if(oh > 23 || om > 59) return NAN;
            offset_min = sign * (oh * 60 + om);
        }else{
            return NAN;
        }
    }
    if(pos != src.size()) return NAN;

    double t = make_time(y, mo, d, h, mi, s, ms);
    if(std::isnan(t)) return NAN;
    return t - (double)offset_min * 60000.0;
}

} // namespace

namespace {

double self_time(Value* self){
    if(!self || self->type != Value::Obj)
        throw new JuaTypeError("date: not a date instance");
    auto obj = static_cast<Object*>(self);
    auto v = obj->getOwn("time");
    if(!v || v->type != Value::Num)
        throw new JuaTypeError("date: not a date instance");
    return static_cast<Number*>(v)->value;
}
double set_time(Value* self, double t){
    static_cast<Object*>(self)->setProp("time", new Number(self->vm, t));
    return t;
}
double num_arg(const jualist& args, size_t i, const char* what){
    auto v = args[i];
    if(v->type != Value::Num)
        throw new JuaTypeError(string("date: ") + what + " must be a number");
    return static_cast<Number*>(v)->value;
}

} // namespace

Object* VM::makeDate(){
    // 标准可构造类：__call 取自 classProto（Object.new），实例通过 init 设置自身属性
    auto proto = new Object(this, classProto);

    // ---- 构造 ----
    proto->setProp("init", makeFunc([this](jualist& args){
        if(args.empty()) throw new JuaTypeError("date.init() requires self");
        Value* self = args[0];
        size_t n = args.size() - 1; // 额外的实参个数
        double t;
        if(n == 0){
            t = now_ms();
        }else if(n == 1){
            if(args[1]->type != Value::Num)
                throw new JuaTypeError("date(ms) requires a number");
            double ms = static_cast<Number*>(args[1])->value;
            t = (ms >= -MAX_TIME && ms <= MAX_TIME) ? ms : NAN;
        }else{
            int64_t f[7] = {0, 0, 1, 0, 0, 0, 0}; // y m d h mi s ms
            for(size_t i=0; i<7 && i+1 < args.size(); i++){
                f[i] = (int64_t)num_arg(args, i+1, "field");
            }
            t = make_time(f[0], f[1], f[2], f[3], f[4], f[5], f[6]);
        }
        set_time(self, t);
        return Null::getInst();
    }));

    // ---- 取值 ----
    auto getter = [this](std::function<double(const Fields&)> pick){
        return makeFunc([this, pick](jualist& args) -> Value* {
            if(args.empty()) throw new JuaTypeError("date getter requires self");
            Fields f;
            double t = self_time(args[0]);
            if(!fields_from_time(t, f)) return new Number(this, NAN);
            return new Number(this, (double)pick(f));
        });
    };
    proto->setProp("getTime", makeFunc([this](jualist& args){
        if(args.empty()) throw new JuaTypeError("date.getTime() requires self");
        return new Number(this, self_time(args[0]));
    }));
    proto->setProp("getFullYear", getter([](const Fields& f){ return (double)f.y; }));
    proto->setProp("getMonth", getter([](const Fields& f){ return (double)f.m; }));
    proto->setProp("getDate", getter([](const Fields& f){ return (double)f.d; }));
    proto->setProp("getDay", getter([](const Fields& f){ return (double)f.wd; }));
    proto->setProp("getHours", getter([](const Fields& f){ return (double)f.h; }));
    proto->setProp("getMinutes", getter([](const Fields& f){ return (double)f.mi; }));
    proto->setProp("getSeconds", getter([](const Fields& f){ return (double)f.s; }));
    proto->setProp("getMilliseconds", getter([](const Fields& f){ return (double)f.ms; }));

    // ---- 设值 ----
    // 共同流程：读当前字段，交给 apply 改字段，再写回
    auto setter = [this](size_t min_args,
                         std::function<void(Fields&, const jualist&)> apply){
        return makeFunc([this, min_args, apply](jualist& args) -> Value* {
            if(args.size() < min_args)
                throw new JuaTypeError("date setter requires more arguments");
            for(size_t i=1; i<args.size(); i++)
                if(args[i]->type != Value::Num)
                    throw new JuaTypeError("date: setter arguments must be numbers");
            Value* self = args[0];
            Fields f;
            double t = self_time(self);
            if(!fields_from_time(t, f)){
                set_time(self, NAN);
                return new Number(this, NAN);
            }
            apply(f, args);
            double nt = make_time(f.y, f.m, f.d, f.h, f.mi, f.s, f.ms);
            set_time(self, nt);
            return new Number(this, nt);
        });
    };
    proto->setProp("setTime", makeFunc([this](jualist& args){
        if(args.size() < 2) throw new JuaTypeError("date.setTime() requires 2 arguments");
        double ms = num_arg(args, 1, "time");
        double t = (ms >= -MAX_TIME && ms <= MAX_TIME) ? ms : NAN;
        set_time(args[0], t);
        return new Number(this, t);
    }));
    proto->setProp("setFullYear", setter(2, [](Fields& f, const jualist& a){
        f.y = (int64_t)static_cast<Number*>(a[1])->value;
        if(a.size() >= 3) f.m = (int64_t)static_cast<Number*>(a[2])->value;
        if(a.size() >= 4) f.d = (int64_t)static_cast<Number*>(a[3])->value;
    }));
    proto->setProp("setMonth", setter(2, [](Fields& f, const jualist& a){
        f.m = (int64_t)static_cast<Number*>(a[1])->value;
        if(a.size() >= 3) f.d = (int64_t)static_cast<Number*>(a[2])->value;
    }));
    proto->setProp("setDate", setter(2, [](Fields& f, const jualist& a){
        f.d = (int64_t)static_cast<Number*>(a[1])->value;
    }));
    proto->setProp("setHours", setter(2, [](Fields& f, const jualist& a){
        f.h = (int64_t)static_cast<Number*>(a[1])->value;
        if(a.size() >= 3) f.mi = (int64_t)static_cast<Number*>(a[2])->value;
        if(a.size() >= 4) f.s = (int64_t)static_cast<Number*>(a[3])->value;
        if(a.size() >= 5) f.ms = (int64_t)static_cast<Number*>(a[4])->value;
    }));
    proto->setProp("setMinutes", setter(2, [](Fields& f, const jualist& a){
        f.mi = (int64_t)static_cast<Number*>(a[1])->value;
        if(a.size() >= 3) f.s = (int64_t)static_cast<Number*>(a[2])->value;
        if(a.size() >= 4) f.ms = (int64_t)static_cast<Number*>(a[3])->value;
    }));
    proto->setProp("setSeconds", setter(2, [](Fields& f, const jualist& a){
        f.s = (int64_t)static_cast<Number*>(a[1])->value;
        if(a.size() >= 3) f.ms = (int64_t)static_cast<Number*>(a[2])->value;
    }));
    proto->setProp("setMilliseconds", setter(2, [](Fields& f, const jualist& a){
        f.ms = (int64_t)static_cast<Number*>(a[1])->value;
    }));

    // ---- 格式化 ----
    proto->setProp("toISOString", makeFunc([this](jualist& args){
        if(args.empty()) throw new JuaTypeError("date.toISOString() requires self");
        double t = self_time(args[0]);
        Fields f;
        if(!fields_from_time(t, f))
            throw new JuaRangeError("date.toISOString() called on an invalid date");
        return new String(this, iso_from_time(t));
    }));
    // 作为「对象转字符串」用的元方法：无效日期给 "Invalid Date"，不报错
    proto->setProp("toString", makeFunc([this](jualist& args){
        if(args.empty()) throw new JuaTypeError("date.toString() requires self");
        return new String(this, iso_from_time(self_time(args[0])));
    }));
    proto->setProp("toJSON", makeFunc([this](jualist& args) -> Value* {
        if(args.empty()) throw new JuaTypeError("date.toJSON() requires self");
        double t = self_time(args[0]);
        Fields f;
        if(!fields_from_time(t, f)) return Null::getInst();
        return new String(this, iso_from_time(t));
    }));

    // ---- 静态方法 ----
    proto->setProp("now", makeFunc([this](jualist&){
        auto obj = new Object(this, DateProto);
        obj->setProp("time", new Number(this, now_ms()));
        return obj;
    }));
    proto->setProp("parse", makeFunc([this](jualist& args){
        if(args.empty()) throw new JuaTypeError("date.parse() requires 1 argument");
        if(args[0]->type != Value::Str)
            throw new JuaTypeError("date.parse() requires a string");
        return new Number(this, parse_iso(args[0]->toString()));
    }));
    proto->setProp("UTC", makeFunc([this](jualist& args){
        if(args.size() < 2) throw new JuaTypeError("date.UTC() requires at least 2 arguments");
        int64_t f[7] = {0, 0, 1, 0, 0, 0, 0};
        for(size_t i=0; i<7 && i<args.size(); i++) f[i] = (int64_t)num_arg(args, i, "field");
        return new Number(this, make_time(f[0], f[1], f[2], f[3], f[4], f[5], f[6]));
    }));

    // ---- 元方法 ----
    proto->setProp("__eq", makeFunc([this](jualist& args){
        if(args.size() < 2) throw new JuaTypeError("date.__eq requires 2 arguments");
        return Boolean::getInst(self_time(args[0]) == self_time(args[1]));
    }));
    auto compare = [this](std::function<bool(double, double)> cmp){
        return makeFunc([this, cmp](jualist& args) -> Value* {
            if(args.size() < 2) throw new JuaTypeError("date comparison requires 2 arguments");
            double a = self_time(args[0]), b = self_time(args[1]);
            if(std::isnan(a) || std::isnan(b)) return Boolean::getInst(false);
            return Boolean::getInst(cmp(a, b));
        });
    };
    proto->setProp("__lt", compare([](double a, double b){ return a < b; }));
    proto->setProp("__le", compare([](double a, double b){ return a <= b; }));
    // 日期 - 日期 得毫秒差；日期 - 数字 得新日期
    proto->setProp("__sub", makeFunc([this](jualist& args) -> Value* {
        if(args.size() < 2) throw new JuaTypeError("date.__sub requires 2 arguments");
        double a = self_time(args[0]);
        auto other = args[1];
        if(other->type == Value::Num){
            double t = a - static_cast<Number*>(other)->value;
            auto obj = new Object(this, DateProto);
            obj->setProp("time", new Number(this, t));
            return obj;
        }
        return new Number(this, a - self_time(other));
    }));

    DateProto = proto;
    return proto;
}
