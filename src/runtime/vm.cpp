#include "synapse/runtime/vm.h"
#include "synapse/runtime/chunk.h"
#include "synapse/runtime/opcodes.h"
#include "synapse/runtime/value.h"
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <new>
#include <algorithm>
#include <cctype>

namespace syn {

thread_local VM* tls_vm = nullptr;

VM::VM()
{
    for (int i = 0; i < REGISTER_STACK; ++i) m_stack[i] = Value::none_val();
    m_globals.reserve(1024);  // keep pointers stable for GET_GLOBAL inline cache
}

VM::~VM()
{
    // Free GC list
    Obj* cur = m_gc_list;
    while (cur) {
        Obj* next = cur->gc_next;
        switch (cur->kind) {
        case ObjKind::String:   delete static_cast<ObjString*>(cur);   break;
        case ObjKind::List:     delete static_cast<ObjList*>(cur);     break;
        case ObjKind::Map:      delete static_cast<ObjMap*>(cur);      break;
        case ObjKind::Function: delete static_cast<ObjFunction*>(cur); break;
        case ObjKind::Closure:  delete static_cast<ObjClosure*>(cur);  break;
        case ObjKind::Upvalue:  delete static_cast<ObjUpvalue*>(cur);  break;
        case ObjKind::Native:   delete static_cast<ObjNative*>(cur);   break;
        case ObjKind::Error:    delete static_cast<ObjError*>(cur);    break;
        case ObjKind::Int:      delete static_cast<ObjInt*>(cur);      break;
        default:                delete cur; break;
        }
        cur = next;
    }
    // Free the object pools (not in gc_list)
    ObjString* sp = m_str_pool;
    while (sp) {
        ObjString* next = static_cast<ObjString*>(sp->gc_next);
        delete sp;
        sp = next;
    }
    ObjList* lp = m_list_pool;
    while (lp) {
        ObjList* next = static_cast<ObjList*>(lp->gc_next);
        delete lp;
        lp = next;
    }
    ObjMap* mp = m_map_pool;
    while (mp) {
        ObjMap* next = static_cast<ObjMap*>(mp->gc_next);
        delete mp;
        mp = next;
    }
}

// ── String pool allocators ────────────────────────────────────────────────────

ObjString* VM::alloc_string(const char* data, size_t len)
{
    ++m_alloc_count;
    ObjString* obj;
    if (m_str_pool) {
        obj = m_str_pool;
        m_str_pool = static_cast<ObjString*>(obj->gc_next);
        obj->gc_mark = false;
        obj->data.assign(data, len);  // reuses buffer if capacity >= len
    } else {
        obj = new ObjString(std::string(data, len));
    }
    obj->gc_next = m_gc_list;
    m_gc_list = obj;
    return obj;
}

ObjString* VM::alloc_string_raw()
{
    ++m_alloc_count;
    ObjString* obj;
    if (m_str_pool) {
        obj = m_str_pool;
        m_str_pool = static_cast<ObjString*>(obj->gc_next);
        obj->gc_mark = false;
        // data already cleared by GC sweep; capacity preserved
    } else {
        obj = new ObjString("");
    }
    obj->gc_next = m_gc_list;
    m_gc_list = obj;
    return obj;
}

// ── List / Map pool allocators ────────────────────────────────────────────────

ObjList* VM::alloc_list()
{
    ++m_alloc_count;
    ObjList* obj;
    if (m_list_pool) {
        obj = m_list_pool;
        m_list_pool = static_cast<ObjList*>(obj->gc_next);
        obj->gc_mark = false;
        obj->kind = ObjKind::List;  // reset — pooled tuples must become lists
        obj->items.clear();
    } else {
        obj = new ObjList();
    }
    obj->gc_next = m_gc_list;
    m_gc_list = obj;
    return obj;
}

ObjMap* VM::alloc_map()
{
    ++m_alloc_count;
    ObjMap* obj;
    if (m_map_pool) {
        obj = m_map_pool;
        m_map_pool = static_cast<ObjMap*>(obj->gc_next);
        obj->gc_mark = false;
        // pairs, str_idx, int_idx already cleaned in GC sweep
    } else {
        obj = new ObjMap();
        obj->pairs.reserve(4);  // avoid realloc for typical 2-4 key maps
    }
    obj->gc_next = m_gc_list;
    m_gc_list = obj;
    return obj;
}

// ── Mark-and-sweep GC ─────────────────────────────────────────────────────────

void VM::mark_value(Value v)
{
    if (v.is_ptr()) { mark_object(v.as_ptr()); return; }
    // ObjInt: pointer in payload, different tag than V_PTR
    if (v.is_large_int())
        mark_object(reinterpret_cast<ObjInt*>(v.raw & VPAY_MASK));
}

void VM::mark_object(Obj* o)
{
    if (!o || o->gc_mark) return;
    o->gc_mark = true;
    switch (o->kind) {
    case ObjKind::List: case ObjKind::Tuple: {
        for (auto& item : static_cast<ObjList*>(o)->items) mark_value(item);
        break;
    }
    case ObjKind::Map: {
        for (auto& [k, v] : static_cast<ObjMap*>(o)->pairs) {
            mark_value(k); mark_value(v);
        }
        break;
    }
    case ObjKind::Function: {
        auto* fn = static_cast<ObjFunction*>(o);
        for (auto& c : fn->chunk->constants) mark_value(c);
        break;
    }
    case ObjKind::Closure: {
        auto* cl = static_cast<ObjClosure*>(o);
        mark_object(cl->fn);
        for (auto* uv : cl->upvalues) mark_object(uv);
        break;
    }
    case ObjKind::Upvalue: {
        auto* uv = static_cast<ObjUpvalue*>(o);
        mark_value(uv->is_closed ? uv->closed : *uv->location);
        break;
    }
    case ObjKind::Error:
        mark_value(static_cast<ObjError*>(o)->message);
        break;
    default: break;
    }
}

void VM::collect_garbage()
{
    // Mark phase — roots: call frames, stack registers, globals, open upvalues
    for (int i = 0; i < m_frame_count; ++i)
        if (m_frames[i].closure) mark_object(m_frames[i].closure);
    // Only scan up to the highest frame's base + max registers per frame (256).
    // Avoids scanning 16k slots when only a few hundred are live.
    int scan_top = 256;
    for (int i = 0; i < m_frame_count; ++i)
        scan_top = m_frames[i].base + 256;
    if (scan_top > REGISTER_STACK) scan_top = REGISTER_STACK;
    for (int i = 0; i < scan_top; ++i) mark_value(m_stack[i]);
    for (auto& [k, v] : m_globals) mark_value(v);
    for (auto* uv = m_open_upvalues; uv; uv = uv->next_open) mark_object(uv);

    // Sweep phase — count live objects during sweep to avoid second traversal
    Obj** cur = &m_gc_list;
    size_t live = 0;
    while (*cur) {
        Obj* o = *cur;
        if (o->gc_mark) {
            o->gc_mark = false;  // reset for next cycle
            cur = &o->gc_next;
            ++live;
        } else {
            *cur = o->gc_next;
            switch (o->kind) {
            case ObjKind::String: {
                auto* s = static_cast<ObjString*>(o);
                s->data.clear();
                s->gc_next = m_str_pool;
                m_str_pool = s;
                break;
            }
            case ObjKind::List: {
                auto* l = static_cast<ObjList*>(o);
                l->items.clear();
                l->gc_next = m_list_pool;
                m_list_pool = l;
                break;
            }
            case ObjKind::Map: {
                auto* m = static_cast<ObjMap*>(o);
                m->pairs.clear();
                delete m->str_idx; m->str_idx = nullptr;
                delete m->int_idx; m->int_idx = nullptr;
                m->gc_next = m_map_pool;
                m_map_pool = m;
                break;
            }
            case ObjKind::Function: delete static_cast<ObjFunction*>(o); break;
            case ObjKind::Closure:  delete static_cast<ObjClosure*>(o);  break;
            case ObjKind::Upvalue:  delete static_cast<ObjUpvalue*>(o);  break;
            case ObjKind::Native:   delete static_cast<ObjNative*>(o);   break;
            case ObjKind::Error:    delete static_cast<ObjError*>(o);    break;
            case ObjKind::Int:      delete static_cast<ObjInt*>(o);      break;
            default:                delete o; break;
            }
        }
    }

    // Next threshold: proportional to live heap, with floor to avoid thrashing
    m_alloc_count = 0;
    m_gc_threshold = std::max(std::min(live * 2 + 256, size_t(65536)), size_t(1024));
}

Value VM::invoke_method_str(Value obj, const std::string& name, int nargs, Value* args)
{
    MethodId mid = resolve_method_id(name);
    return invoke_method(obj, mid, nargs, args);
}

Value VM::invoke_method(Value obj, MethodId mid, int nargs, Value* args)
{
    // ── List methods ──────────────────────────────────────────────────────────
    if (val_is_list(obj)) {
        ObjList& lst = as_list(obj);
        switch (mid) {
        case MethodId::Append:
            if (nargs >= 1) lst.items.push_back(args[0]);
            return Value::none_val();
        case MethodId::Pop:
            if (lst.items.empty()) return Value::none_val();
            { Value v = lst.items.back(); lst.items.pop_back(); return v; }
        case MethodId::Len:
            return Value::from_int(int64_t(lst.items.size()));
        case MethodId::Reverse:
            std::reverse(lst.items.begin(), lst.items.end());
            return Value::none_val();
        case MethodId::Sort:
            std::sort(lst.items.begin(), lst.items.end(), [](const Value& a, const Value& b) {
                if (a.is_int() && b.is_int()) return a.as_int() < b.as_int();
                double da = a.is_float() ? a.as_float() : double(a.is_int() ? a.as_int() : 0);
                double db = b.is_float() ? b.as_float() : double(b.is_int() ? b.as_int() : 0);
                return da < db;
            });
            return Value::none_val();
        case MethodId::Index:
            if (nargs < 1) return Value::from_int(-1);
            for (int64_t i = 0; i < int64_t(lst.items.size()); ++i)
                if (val_eq(lst.items[i], args[0])) return Value::from_int(i);
            return Value::from_int(-1);
        case MethodId::Contains:
            if (nargs < 1) return Value::from_bool(false);
            for (auto& item : lst.items)
                if (val_eq(item, args[0])) return Value::from_bool(true);
            return Value::from_bool(false);
        case MethodId::Extend:
            if (nargs >= 1 && val_is_list(args[0]))
                for (auto& v : as_list(args[0]).items) lst.items.push_back(v);
            return Value::none_val();
        default: return Value::none_val();
        }
    }

    // ── String methods ────────────────────────────────────────────────────────
    if (val_is_string(obj)) {
        const std::string& s = as_str(obj).data;
        switch (mid) {
        case MethodId::Upper: {
            ObjString* r = alloc_string_raw();
            r->data.resize(s.size());
            for (size_t i = 0; i < s.size(); ++i)
                r->data[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[i])));
            return Value::from_ptr(r);
        }
        case MethodId::Lower: {
            ObjString* r = alloc_string_raw();
            r->data.resize(s.size());
            for (size_t i = 0; i < s.size(); ++i)
                r->data[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
            return Value::from_ptr(r);
        }
        case MethodId::Trim:
        case MethodId::Strip: {
            size_t lo = s.find_first_not_of(" \t\r\n");
            if (lo == std::string::npos) return Value::from_ptr(alloc_string("", 0));
            size_t hi = s.find_last_not_of(" \t\r\n");
            return Value::from_ptr(alloc_string(s.data() + lo, hi - lo + 1));
        }
        case MethodId::Split: {
            static const std::string default_sep = " ";
            const std::string& sep = (nargs >= 1 && val_is_string(args[0])) ? as_str(args[0]).data : default_sep;
            auto* list = alloc_list();
            if (sep.empty()) {
                for (char c : s) list->items.push_back(Value::from_ptr(alloc_string(&c, 1)));
                return Value::from_ptr(list);
            }
            size_t pos = 0, found;
            while ((found = s.find(sep, pos)) != std::string::npos) {
                list->items.push_back(Value::from_ptr(alloc_string(s.data() + pos, found - pos)));
                pos = found + sep.size();
            }
            list->items.push_back(Value::from_ptr(alloc_string(s.data() + pos, s.size() - pos)));
            return Value::from_ptr(list);
        }
        case MethodId::Join: {
            if (nargs < 1 || !val_is_list(args[0])) return Value::from_ptr(alloc_string("", 0));
            const auto& items = as_list(args[0]).items;
            ObjString* r = alloc_string_raw();
            for (size_t i = 0; i < items.size(); ++i) {
                if (i) r->data += s;
                if (val_is_string(items[i])) r->data += as_str(items[i]).data;
                else r->data += val_to_string(items[i]);
            }
            return Value::from_ptr(r);
        }
        case MethodId::Find: {
            if (nargs < 1 || !val_is_string(args[0])) return Value::from_int(-1);
            int64_t start = (nargs >= 2 && args[1].is_int()) ? args[1].as_int() : 0;
            auto pos = s.find(as_str(args[0]).data, size_t(start));
            return Value::from_int(pos == std::string::npos ? -1 : int64_t(pos));
        }
        case MethodId::Replace: {
            if (nargs < 2 || !val_is_string(args[0]) || !val_is_string(args[1])) return obj;
            ObjString* r = alloc_string_raw();
            r->data = s;
            const std::string& old_s = as_str(args[0]).data;
            const std::string& new_s = as_str(args[1]).data;
            if (!old_s.empty()) {
                size_t pos = 0;
                while ((pos = r->data.find(old_s, pos)) != std::string::npos) {
                    r->data.replace(pos, old_s.size(), new_s);
                    pos += new_s.size();
                }
            }
            return Value::from_ptr(r);
        }
        case MethodId::StartsWith: {
            if (nargs < 1 || !val_is_string(args[0])) return Value::from_bool(false);
            const std::string& p = as_str(args[0]).data;
            return Value::from_bool(s.size() >= p.size() && s.compare(0, p.size(), p) == 0);
        }
        case MethodId::EndsWith: {
            if (nargs < 1 || !val_is_string(args[0])) return Value::from_bool(false);
            const std::string& p = as_str(args[0]).data;
            return Value::from_bool(s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0);
        }
        case MethodId::Substr: {
            int64_t len = int64_t(s.size());
            int64_t lo = (nargs >= 1 && args[0].is_int()) ? args[0].as_int() : 0;
            int64_t hi = (nargs >= 2 && args[1].is_int()) ? args[1].as_int() : len;
            if (lo < 0) lo += len; if (hi < 0) hi += len;
            lo = std::max(int64_t(0), std::min(lo, len));
            hi = std::max(int64_t(0), std::min(hi, len));
            if (lo >= hi) return Value::from_ptr(alloc_string("", 0));
            return Value::from_ptr(alloc_string(s.data() + lo, size_t(hi - lo)));
        }
        case MethodId::Len: return Value::from_int(int64_t(s.size()));
        case MethodId::Ord: return Value::from_int(s.empty() ? 0 : int64_t(static_cast<unsigned char>(s[0])));
        case MethodId::Count: {
            if (nargs < 1 || !val_is_string(args[0])) return Value::from_int(0);
            const std::string& sub = as_str(args[0]).data;
            if (sub.empty()) return Value::from_int(int64_t(s.size() + 1));
            int64_t cnt = 0;
            size_t pos = 0;
            while ((pos = s.find(sub, pos)) != std::string::npos) { ++cnt; pos += sub.size(); }
            return Value::from_int(cnt);
        }
        default: return Value::none_val();
        }
    }

    // ── Map methods ───────────────────────────────────────────────────────────
    if (val_is_map(obj)) {
        ObjMap& m = as_map(obj);
        switch (mid) {
        case MethodId::Keys: {
            auto* list = alloc_list();
            for (auto& [k, v] : m.pairs) list->items.push_back(k);
            return Value::from_ptr(list);
        }
        case MethodId::Values: {
            auto* list = alloc_list();
            for (auto& [k, v] : m.pairs) list->items.push_back(v);
            return Value::from_ptr(list);
        }
        case MethodId::Has:
            if (nargs < 1) return Value::from_bool(false);
            for (auto& [k, v] : m.pairs) if (val_eq(k, args[0])) return Value::from_bool(true);
            return Value::from_bool(false);
        case MethodId::Get:
            if (nargs < 1) return Value::none_val();
            for (auto& [k, v] : m.pairs) if (val_eq(k, args[0])) return v;
            return nargs >= 2 ? args[1] : Value::none_val();
        case MethodId::Len: return Value::from_int(int64_t(m.pairs.size()));
        default: return Value::none_val();
        }
    }

    return Value::none_val();
}

void VM::define_native(const std::string& name, NativeFn fn)
{
    auto* obj = alloc<ObjNative>(name, std::move(fn));
    m_globals[name] = Value::from_ptr(obj);
}

void VM::define_global(const std::string& name, Value v)
{
    m_globals[name] = v;
}

// ── Upvalue management ────────────────────────────────────────────────────────

ObjUpvalue* VM::capture_upvalue(Value* slot)
{
    ObjUpvalue* prev = nullptr;
    ObjUpvalue* cur  = m_open_upvalues;
    while (cur && cur->location > slot) { prev = cur; cur = cur->next_open; }
    if (cur && cur->location == slot) return cur;

    auto* uv = alloc<ObjUpvalue>(slot);
    uv->next_open = cur;
    if (prev) prev->next_open = uv; else m_open_upvalues = uv;
    return uv;
}

void VM::close_upvalues(Value* last)
{
    while (m_open_upvalues && m_open_upvalues->location >= last) {
        ObjUpvalue* uv = m_open_upvalues;
        uv->close();
        m_open_upvalues = uv->next_open;
    }
}

// ── Call dispatch ────────────────────────────────────────────────────────────

Value VM::call_value(Value callee, int nargs, Value* args)
{
    if (!callee.is_ptr()) throw std::runtime_error("cannot call non-function");
    Obj* o = callee.as_ptr();

    if (o->kind == ObjKind::Native) {
        return static_cast<ObjNative*>(o)->fn(nargs, args);
    }

    ObjClosure* closure = nullptr;
    if (o->kind == ObjKind::Closure) {
        closure = static_cast<ObjClosure*>(o);
    } else if (o->kind == ObjKind::Function) {
        // wrap bare function
        closure = alloc<ObjClosure>(static_cast<ObjFunction*>(o));
    } else {
        throw std::runtime_error("not callable");
    }

    if (m_frame_count >= MAX_FRAMES) throw std::runtime_error("stack overflow");

    int base = int(args - m_stack);
    CallFrame& frame = m_frames[m_frame_count++];
    frame.closure = closure;
    frame.chunk   = closure->fn->chunk;
    frame.pc      = closure->fn->chunk->code.data();
    frame.regs    = args;
    frame.base    = base;

    Value result = run_frame(frame);
    --m_frame_count;
    return result;
}

// ── Main execution loop ───────────────────────────────────────────────────────

// Computed goto dispatch — GCC/Clang extension
#if defined(__GNUC__) || defined(__clang__)
#  define DISPATCH_TABLE 1
#endif

Value VM::run_frame(CallFrame& outer_frame)
{
    // entry_depth: return from C++ when frame stack drops below this
    const int entry_depth = m_frame_count;

    CallFrame* frame = &outer_frame;
    uint64_t*  pc    = frame->pc;
    Value*     regs  = frame->regs;
    Chunk*     ck    = frame->chunk;


#ifdef DISPATCH_TABLE
    // Computed-goto dispatch table — fastest path
    static const void* dispatch[] = {
        // This table maps Op values (uint8_t) to labels.
        // We fill it dynamically the first time through.
        nullptr
    };

    // ponytail: fall back to switch — computed goto needs dense table setup;
    // switch is easier to maintain and plenty fast with -O2 branch prediction.
#endif

#define NEXT_INS() (*pc++)
#define REGS regs

    while (true) {
        if (__builtin_expect(m_alloc_count >= m_gc_threshold, 0)) collect_garbage();
        uint64_t w = NEXT_INS();
        Op op = INS_OP(w);

        switch (op) {

        // ── Loads ────────────────────────────────────────────────────────────
        case Op::LOAD_INT: {
            uint8_t a = INS_A(w);
            REGS[a] = Value::from_int(INS_IMM48(w));
            break;
        }
        case Op::LOAD_FLOAT: {
            // B = pool index
            uint8_t a = INS_A(w), b = INS_B(w);
            REGS[a] = ck->constants[b];
            break;
        }
        case Op::LOAD_TRUE:  REGS[INS_A(w)] = Value::from_bool(true);  break;
        case Op::LOAD_FALSE: REGS[INS_A(w)] = Value::from_bool(false); break;
        case Op::LOAD_NONE:  REGS[INS_A(w)] = Value::none_val();       break;
        case Op::LOAD_DURATION: {
            uint8_t a = INS_A(w);
            REGS[a] = Value::from_duration(INS_IMM48(w));
            break;
        }
        case Op::LOAD_CONST: {
            uint8_t a = INS_A(w);
            REGS[a] = ck->constants[INS_IMM48(w)];
            break;
        }
        case Op::MOVE: {
            uint8_t a = INS_A(w), b = INS_B(w);
            REGS[a] = REGS[b];
            break;
        }

        // ── Globals ──────────────────────────────────────────────────────────
        case Op::GET_GLOBAL: {
            uint8_t a = INS_A(w);
            int64_t ki = INS_IMM48(w);
            Value* cached = ck->global_cache[ki];
            if (cached) { REGS[a] = *cached; break; }
            const std::string& name = as_str(ck->constants[ki]).data;
            auto it = m_globals.find(name);
            if (it == m_globals.end())
                throw std::runtime_error("undefined variable: " + name);
            ck->global_cache[ki] = &it->second;
            REGS[a] = it->second;
            break;
        }
        case Op::SET_GLOBAL: {
            uint8_t a = INS_A(w);
            int64_t ki = INS_IMM48(w);
            Value* cached = ck->global_cache[ki];
            if (__builtin_expect(cached != nullptr, 1)) {
                *cached = REGS[a];  // write through stable cached pointer
            } else {
                const std::string& name = as_str(ck->constants[ki]).data;
                auto it = m_globals.find(name);
                if (it != m_globals.end()) {
                    it->second = REGS[a];
                    ck->global_cache[ki] = &it->second;  // now cache the stable pointer
                } else {
                    m_globals[name] = REGS[a];
                    // leave cache null: new insertion may move other iterators
                }
            }
            break;
        }

        // ── Upvalues ─────────────────────────────────────────────────────────
        case Op::GET_UPVAL: {
            uint8_t a = INS_A(w), b = INS_B(w);
            REGS[a] = frame->closure->upvalues[b]->get();
            break;
        }
        case Op::SET_UPVAL: {
            uint8_t a = INS_A(w), b = INS_B(w);
            frame->closure->upvalues[b]->set(REGS[a]);
            break;
        }
        case Op::CLOSE_UPVAL: {
            close_upvalues(&REGS[INS_A(w)]);
            break;
        }

        // ── Arithmetic ───────────────────────────────────────────────────────
        case Op::ADD:  {
            uint8_t a=INS_A(w),b=INS_B(w),c=INS_C(w);
            // ponytail: in-place string append when dest==src (s=s+x or s+=x)
            if (a == b && val_is_string(REGS[a]) && val_is_string(REGS[c])) {
                as_str(REGS[a]).data.append(as_str(REGS[c]).data);
                as_str(REGS[a]).hash = 0;
                break;
            }
            Value r = val_add(REGS[b],REGS[c]);
            REGS[a] = r;
            if (r.is_ptr()) { // new heap obj from string concat — register with GC
                Obj* o = r.as_ptr(); o->gc_next = m_gc_list; m_gc_list = o;
                ++m_alloc_count;
            }
            break;
        }
        case Op::SUB:  { uint8_t a=INS_A(w),b=INS_B(w),c=INS_C(w); REGS[a]=val_sub(REGS[b],REGS[c]); break; }
        case Op::MUL:  { uint8_t a=INS_A(w),b=INS_B(w),c=INS_C(w); REGS[a]=val_mul(REGS[b],REGS[c]); break; }
        case Op::DIV:  { uint8_t a=INS_A(w),b=INS_B(w),c=INS_C(w); REGS[a]=val_div(REGS[b],REGS[c]); break; }
        case Op::IDIV: { uint8_t a=INS_A(w),b=INS_B(w),c=INS_C(w); REGS[a]=val_idiv(REGS[b],REGS[c]); break; }
        case Op::MOD:  { uint8_t a=INS_A(w),b=INS_B(w),c=INS_C(w); REGS[a]=val_mod(REGS[b],REGS[c]); break; }
        case Op::POW:  { uint8_t a=INS_A(w),b=INS_B(w),c=INS_C(w); REGS[a]=val_pow(REGS[b],REGS[c]); break; }
        case Op::UNM:  { uint8_t a=INS_A(w),b=INS_B(w); REGS[a]=val_unm(REGS[b]); break; }

        // R×imm variants
        case Op::ADDI: { uint8_t a=INS_A(w),b=INS_B(w); REGS[a]=val_add(REGS[b],Value::from_int(INS_IMM40(w))); break; }
        case Op::SUBI: { uint8_t a=INS_A(w),b=INS_B(w); REGS[a]=val_sub(REGS[b],Value::from_int(INS_IMM40(w))); break; }
        case Op::MULI: { uint8_t a=INS_A(w),b=INS_B(w); REGS[a]=val_mul(REGS[b],Value::from_int(INS_IMM40(w))); break; }
        case Op::DIVI: { uint8_t a=INS_A(w),b=INS_B(w); REGS[a]=val_div(REGS[b],Value::from_int(INS_IMM40(w))); break; }
        case Op::IDIVI:{ uint8_t a=INS_A(w),b=INS_B(w); REGS[a]=val_idiv(REGS[b],Value::from_int(INS_IMM40(w)));break; }
        case Op::MODI: { uint8_t a=INS_A(w),b=INS_B(w); REGS[a]=val_mod(REGS[b],Value::from_int(INS_IMM40(w))); break; }
        case Op::POWI: { uint8_t a=INS_A(w),b=INS_B(w); REGS[a]=val_pow(REGS[b],Value::from_int(INS_IMM40(w))); break; }

        // ── Comparison ───────────────────────────────────────────────────────
        case Op::EQ:  { uint8_t a=INS_A(w),b=INS_B(w),c=INS_C(w); REGS[a]=Value::from_bool(val_eq(REGS[b],REGS[c]));  break; }
        case Op::NEQ: { uint8_t a=INS_A(w),b=INS_B(w),c=INS_C(w); REGS[a]=Value::from_bool(!val_eq(REGS[b],REGS[c])); break; }
        case Op::LT:  { uint8_t a=INS_A(w),b=INS_B(w),c=INS_C(w); REGS[a]=Value::from_bool(val_lt(REGS[b],REGS[c]));  break; }
        case Op::LTE: { uint8_t a=INS_A(w),b=INS_B(w),c=INS_C(w); REGS[a]=Value::from_bool(val_lte(REGS[b],REGS[c])); break; }
        case Op::NOT: { uint8_t a=INS_A(w),b=INS_B(w); REGS[a]=Value::from_bool(!REGS[b].truthy()); break; }

        // ── Fused compare-branch ─────────────────────────────────────────────
        case Op::JEQ:  { uint8_t a=INS_A(w),b=INS_B(w); if( val_eq(REGS[a],REGS[b])) pc+=INS_OFF32(w); break; }
        case Op::JNEQ: { uint8_t a=INS_A(w),b=INS_B(w); if(!val_eq(REGS[a],REGS[b])) pc+=INS_OFF32(w); break; }
        case Op::JLT:  { uint8_t a=INS_A(w),b=INS_B(w); if( val_lt(REGS[a],REGS[b])) pc+=INS_OFF32(w); break; }
        case Op::JLTE: { uint8_t a=INS_A(w),b=INS_B(w); if( val_lte(REGS[a],REGS[b]))pc+=INS_OFF32(w); break; }
        case Op::JGT:  { uint8_t a=INS_A(w),b=INS_B(w); if(!val_lte(REGS[a],REGS[b]))pc+=INS_OFF32(w); break; }
        case Op::JGTE: { uint8_t a=INS_A(w),b=INS_B(w); if(!val_lt(REGS[a],REGS[b])) pc+=INS_OFF32(w); break; }
        case Op::JT:   { uint8_t a=INS_A(w); if( REGS[a].truthy())  pc+=INS_IMM40(w); break; }
        case Op::JF:   { uint8_t a=INS_A(w); if(!REGS[a].truthy())  pc+=INS_IMM40(w); break; }
        case Op::JNIL: { uint8_t a=INS_A(w); if( REGS[a].is_none()) pc+=INS_IMM40(w); break; }
        case Op::JNNIL:{ uint8_t a=INS_A(w); if(!REGS[a].is_none()) pc+=INS_IMM40(w); break; }
        case Op::JUMP: { pc += INS_IMM48(w); break; }

        // ── Strings ──────────────────────────────────────────────────────────
        case Op::CONCAT: {
            uint8_t a=INS_A(w),b=INS_B(w),c=INS_C(w);
            REGS[a] = val_add(REGS[b], REGS[c]);
            break;
        }
        case Op::CONCAT_N: {
            uint8_t a=INS_A(w), b=INS_B(w), n=INS_C(w);
            ObjString* obj = alloc_string_raw();
            for (int i = 0; i < n; ++i) {
                if (val_is_string(REGS[b+i])) obj->data += as_str(REGS[b+i]).data;
                else obj->data += val_to_string(REGS[b+i]);
            }
            REGS[a] = Value::from_ptr(obj);
            break;
        }
        case Op::INTERP: {
            uint8_t a=INS_A(w), b=INS_B(w), n=INS_C(w);
            ObjString* obj = alloc_string_raw();
            for (int i = 0; i < n; ++i) {
                if (val_is_string(REGS[b+i])) obj->data += as_str(REGS[b+i]).data;
                else obj->data += val_to_string(REGS[b+i]);
            }
            REGS[a] = Value::from_ptr(obj);
            break;
        }
        case Op::STR_LEN: {
            uint8_t a=INS_A(w),b=INS_B(w);
            if (val_is_string(REGS[b]))
                REGS[a] = Value::from_int(int64_t(as_str(REGS[b]).data.size()));
            else if (val_is_list(REGS[b]))
                REGS[a] = Value::from_int(int64_t(as_list(REGS[b]).items.size()));
            else
                throw std::runtime_error("len() on non-string/list");
            break;
        }

        // ── Collections ──────────────────────────────────────────────────────
        case Op::NEW_LIST: {
            uint8_t a=INS_A(w), b=INS_B(w), n=INS_C(w);
            auto* list = alloc_list();
            list->items.reserve(n);
            for (int i = 0; i < n; ++i) list->items.push_back(REGS[b+i]);
            REGS[a] = Value::from_ptr(list);
            break;
        }
        case Op::NEW_MAP: {
            uint8_t a=INS_A(w);
            auto* map = alloc_map();
            REGS[a] = Value::from_ptr(map);
            break;
        }
        case Op::NEW_TUPLE: {
            // ponytail: store as ObjList with Tuple kind
            uint8_t a=INS_A(w), b=INS_B(w), n=INS_C(w);
            auto* list = alloc_list();
            list->kind = ObjKind::Tuple;
            for (int i = 0; i < n; ++i) list->items.push_back(REGS[b+i]);
            REGS[a] = Value::from_ptr(list);
            break;
        }
        case Op::GET_FIELD: {
            uint8_t a=INS_A(w),b=INS_B(w),c=INS_C(w);
            Value obj = REGS[b]; Value key = REGS[c];
            if (val_is_map(obj))  { REGS[a] = as_map(obj).get(key); break; }
            if (obj.is_ptr()) {
                auto* lo = static_cast<ObjList*>(obj.as_ptr());
                if (uint8_t(lo->kind) - 1u <= 1u) {
                    if (!key.is_int()) throw std::runtime_error("list index must be int");
                    int64_t idx = key.as_int();
                    auto& v = lo->items;
                    if (idx < 0) idx += int64_t(v.size());
                    if (idx < 0 || idx >= int64_t(v.size()))
                        throw std::runtime_error("list index out of range");
                    REGS[a] = v[size_t(idx)];
                    break;
                }
            }
            throw std::runtime_error("cannot index this type");
        }
        case Op::SET_FIELD: {
            // A=val B=obj C=key
            uint8_t a=INS_A(w),b=INS_B(w),c=INS_C(w);
            Value obj = REGS[b]; Value key = REGS[c]; Value val = REGS[a];
            if (val_is_map(obj))  { as_map(obj).set(key, val); break; }
            if (obj.is_ptr() && obj.as_ptr()->kind <= ObjKind::Tuple) {
                if (!key.is_int()) throw std::runtime_error("list index must be int");
                int64_t idx = key.as_int();
                auto& v = static_cast<ObjList*>(obj.as_ptr())->items;
                if (idx < 0) idx += int64_t(v.size());
                if (idx < 0 || idx >= int64_t(v.size()))
                    throw std::runtime_error("list index out of range");
                v[idx] = val;
                break;
            }
            throw std::runtime_error("cannot index-assign this type");
        }
        case Op::GET_FIELDK: {
            uint8_t a=INS_A(w),b=INS_B(w);
            Value key = ck->constants[INS_IMM48(w)];
            Value obj = REGS[b];
            if (!val_is_map(obj)) throw std::runtime_error("field access on non-map");
            ObjMap& m = as_map(obj);
            if (!m.str_idx && !m.int_idx) {
                // Small map: monomorphic inline cache
                size_t ins_off = size_t(pc - 1 - ck->code.data());
                uint32_t cached = ck->field_cache[ins_off];
                if (cached) {
                    uint32_t idx = cached - 1;
                    if (idx < uint32_t(m.pairs.size()) && m.pairs[idx].first.raw == key.raw) {
                        REGS[a] = m.pairs[idx].second; break;
                    }
                }
                // Slow path: linear scan + cache update
                Value result = Value::none_val();
                for (uint32_t i = 0; i < uint32_t(m.pairs.size()); ++i) {
                    if (val_eq(m.pairs[i].first, key)) {
                        ck->field_cache[ins_off] = i + 1;
                        result = m.pairs[i].second; break;
                    }
                }
                REGS[a] = result;
            } else {
                REGS[a] = m.get(key);
            }
            break;
        }
        case Op::SET_FIELDK: {
            uint8_t a=INS_A(w),b=INS_B(w);
            Value key = ck->constants[INS_IMM48(w)];
            if (!val_is_map(REGS[b])) throw std::runtime_error("field assign on non-map");
            ObjMap& m = as_map(REGS[b]);
            if (!m.str_idx && !m.int_idx) {
                // Small map: monomorphic inline cache (fast update of existing key)
                size_t ins_off = size_t(pc - 1 - ck->code.data());
                uint32_t cached = ck->field_cache[ins_off];
                if (cached) {
                    uint32_t idx = cached - 1;
                    if (idx < uint32_t(m.pairs.size()) && m.pairs[idx].first.raw == key.raw) {
                        m.pairs[idx].second = REGS[a]; break;
                    }
                }
                m.set(key, REGS[a]);  // slow path: may insert or update
                // Update cache after set
                for (uint32_t i = 0; i < uint32_t(m.pairs.size()); ++i) {
                    if (m.pairs[i].first.raw == key.raw) {
                        ck->field_cache[ins_off] = i + 1; break;
                    }
                }
            } else {
                m.set(key, REGS[a]);
            }
            break;
        }
        case Op::GETI: {
            uint8_t a=INS_A(w),b=INS_B(w);
            int64_t idx = INS_IMM40(w);
            Value obj = REGS[b];
            if (__builtin_expect(obj.is_ptr(), 1)) {
                Obj* o = obj.as_ptr();
                // List=1, Tuple=2: (kind-1) <= 1u catches both, excludes String=0 and Map=3+
                if (__builtin_expect(uint8_t(o->kind) - 1u <= 1u, 1)) {
                    auto& v = static_cast<ObjList*>(o)->items;
                    if (idx < 0) idx += int64_t(v.size());
                    REGS[a] = v[size_t(idx)];
                    break;
                }
            }
            throw std::runtime_error("cannot integer-index this type");
        }
        case Op::SETI: {
            uint8_t a=INS_A(w),b=INS_B(w);
            int64_t idx = INS_IMM40(w);
            if (val_is_list(REGS[a])) {
                auto& v = as_list(REGS[a]).items;
                if (idx < 0) idx += int64_t(v.size());
                v.at(idx) = REGS[b];
            } else {
                throw std::runtime_error("cannot integer-index assign this type");
            }
            break;
        }
        case Op::APPEND: {
            uint8_t a=INS_A(w),b=INS_B(w);
            if (!val_is_list(REGS[a])) throw std::runtime_error("append on non-list");
            as_list(REGS[a]).items.push_back(REGS[b]);
            break;
        }
        case Op::HAS_KEY: {
            uint8_t a=INS_A(w),b=INS_B(w),c=INS_C(w);
            if (val_is_map(REGS[b])) {
                REGS[a] = Value::from_bool(!as_map(REGS[b]).get(REGS[c]).is_none());
            } else if (REGS[b].is_ptr() && uint8_t(REGS[b].as_ptr()->kind) - 1u <= 1u) {
                bool found = false;
                for (auto& x : static_cast<ObjList*>(REGS[b].as_ptr())->items)
                    if (val_eq(x, REGS[c])) { found=true; break; }
                REGS[a] = Value::from_bool(found);
            } else {
                throw std::runtime_error("'in' on non-collection");
            }
            break;
        }
        case Op::MAP_SETK: {
            uint8_t a=INS_A(w),b=INS_B(w);
            Value key = ck->constants[INS_IMM48(w)];
            if (!val_is_map(REGS[a])) throw std::runtime_error("map assign on non-map");
            as_map(REGS[a]).set(key, REGS[b]);
            break;
        }

        // ── Loops ────────────────────────────────────────────────────────────
        case Op::RANGE_PREP: {
            // R[A]=start, R[A+1]=stop, R[A+2]=step  (all from inline s16)
            uint8_t a = INS_A(w);
            REGS[a]   = Value::from_int(INS_S16_A(w));
            REGS[a+1] = Value::from_int(INS_S16_B(w));
            REGS[a+2] = Value::from_int(INS_S16_C(w));
            break;
        }
        case Op::RANGE_PREP_R: {
            uint8_t a=INS_A(w),b=INS_B(w),c=INS_C(w);
            REGS[a]   = REGS[b];
            REGS[a+1] = REGS[c];
            REGS[a+2] = Value::from_int(1);
            break;
        }
        case Op::RANGE_PREP_RS: {
            // A=dest, B=start, C=stop; step in D
            uint8_t a=INS_A(w),b=INS_B(w),c=INS_C(w),d=INS_D(w);
            REGS[a]   = REGS[b];
            REGS[a+1] = REGS[c];
            REGS[a+2] = REGS[d];
            break;
        }
        case Op::RANGE_STEP: {
            // R[A] += R[A+2]; if R[A] >= R[A+1]: jump
            uint8_t a = INS_A(w);
            REGS[a] = val_add(REGS[a], REGS[a+2]);
            bool done;
            int64_t step_sign = REGS[a+2].is_int() ? REGS[a+2].as_int() : int64_t(REGS[a+2].as_float());
            if (step_sign > 0) done = val_lte(REGS[a+1], REGS[a]);
            else               done = val_lte(REGS[a],    REGS[a+1]);
            if (done) pc += INS_IMM40(w);
            break;
        }
        case Op::FOR_PREP: {
            // ponytail: generic iterator — only lists supported now
            uint8_t a=INS_A(w),b=INS_B(w);
            // R[A] = iterator state (list index = 0, stash list in R[A+1])
            REGS[a+1] = REGS[b];
            REGS[a]   = Value::from_int(0);
            break;
        }
        case Op::FOR_STEP: {
            // R[A] is the loop var, R[A+1] is the list, R[A+2] is the index counter
            // actually: R[A]=counter, R[A+1]=list, R[A+2]=output element
            uint8_t a = INS_A(w);
            Value list = REGS[a+1];
            int64_t idx = REGS[a].as_int();
            if (!val_is_list(list) || idx >= int64_t(as_list(list).items.size())) {
                pc += INS_IMM40(w); // exit loop
            } else {
                REGS[a+2] = as_list(list).items[idx];
                REGS[a] = Value::from_int(idx + 1);
            }
            break;
        }

        // ── Functions ────────────────────────────────────────────────────────
        case Op::CLOSURE: {
            uint8_t a = INS_A(w);
            int64_t ki = INS_IMM48(w);
            Value proto = ck->constants[ki];
            ObjFunction* fn = static_cast<ObjFunction*>(proto.as_ptr());
            ObjClosure* closure = alloc<ObjClosure>(fn);
            // Capture upvalues — encoded in following instructions
            for (int i = 0; i < fn->upvalue_count; ++i) {
                uint64_t uv_ins = *pc++;
                bool is_local = bool(INS_A(uv_ins));
                int  idx      = int(INS_B(uv_ins));
                if (is_local) {
                    closure->upvalues[i] = capture_upvalue(&regs[idx]);
                } else {
                    closure->upvalues[i] = frame->closure->upvalues[idx];
                }
            }
            REGS[a] = Value::from_ptr(closure);
            break;
        }
        case Op::CALL: {
            uint8_t a = INS_A(w), nargs = INS_B(w), nret = INS_C(w);
            Value callee_v = REGS[a];
            if (!callee_v.is_ptr()) throw std::runtime_error("cannot call non-function");
            Obj* co = callee_v.as_ptr();

            if (co->kind == ObjKind::Native) {
                Value r = static_cast<ObjNative*>(co)->fn(nargs, &REGS[a+1]);
                if (nret > 0) REGS[a] = r;
                break;
            }

            ObjClosure* cl;
            if (co->kind == ObjKind::Closure) {
                cl = static_cast<ObjClosure*>(co);
                // JIT fast path: bypass interpreter entirely.
                // jit_cache: 0=unchecked, 1=no JIT, else=JitEntry*
                JitEntry* je = static_cast<JitEntry*>(cl->jit_cache);
                if (je == nullptr) {
                    auto jit_it = m_jit.find(cl->fn->name);
                    je = (jit_it != m_jit.end()) ? &jit_it->second
                                                  : reinterpret_cast<JitEntry*>(uintptr_t(1));
                    cl->jit_cache = je;
                }
                if (reinterpret_cast<uintptr_t>(je) > 1) {
                    if (je->int_fn) {
                        bool ok = true;
                        for (int i = 0; i < nargs && ok; ++i)
                            if (!REGS[a + 1 + i].is_int()) ok = false;
                        if (ok) {
                            int64_t r = je->int_fn(nargs, &REGS[a + 1]);
                            if (nret > 0) REGS[a] = Value::from_int(r);
                            break;
                        }
                    }
                    if (je->float_fn) {
                        bool ok = true;
                        for (int i = 0; i < nargs && ok; ++i)
                            if (!REGS[a + 1 + i].is_float()) ok = false;
                        if (ok) {
                            double r = je->float_fn(nargs, &REGS[a + 1]);
                            if (nret > 0) REGS[a] = Value::from_float(r);
                            break;
                        }
                    }
                }
            } else if (co->kind == ObjKind::Function) {
                cl = alloc<ObjClosure>(static_cast<ObjFunction*>(co));
            } else {
                throw std::runtime_error("not callable");
            }

            if (m_frame_count >= MAX_FRAMES)
                throw std::runtime_error("stack overflow");

            frame->pc = pc;  // save caller's next-instruction pointer

            // New frame's regs start at arg0; regs[-1] = caller's REGS[a] = result slot
            Value* new_regs = &REGS[a + 1];
            CallFrame& nf = m_frames[m_frame_count++];
            nf.closure = cl;
            nf.chunk   = cl->fn->chunk;
            nf.pc      = cl->fn->chunk->code.data();
            nf.regs    = new_regs;
            nf.base    = int(new_regs - m_stack);

            frame = &nf;
            pc    = nf.pc;
            regs  = new_regs;
            ck    = nf.chunk;
            break;
        }
        case Op::CALL_0: {
            uint8_t a=INS_A(w),b=INS_B(w);
            REGS[a] = call_value(REGS[b], 0, &REGS[b+1]);
            break;
        }
        case Op::CALL_1: {
            uint8_t a=INS_A(w),b=INS_B(w),c=INS_C(w);
            REGS[a] = call_value(REGS[b], 1, &REGS[c]);
            break;
        }
        case Op::CALL_N: {
            uint8_t a=INS_A(w), n=INS_B(w);
            call_value(REGS[a], n, &REGS[a+1]);
            break;
        }
        case Op::INVOKE: {
            uint8_t a = INS_A(w), nargs = INS_B(w);
            uint32_t key = uint32_t(w & 0xFFFFFFFFu);
            if (key & 0x80000000u) {
                // fallback: key is const pool index for unknown method name
                const std::string& name = as_str(frame->chunk->constants[key & 0x7FFFFFFFu]).data;
                REGS[a] = invoke_method_str(REGS[a], name, int(nargs), &REGS[a + 1]);
            } else {
                REGS[a] = invoke_method(REGS[a], MethodId(key), int(nargs), &REGS[a + 1]);
            }
            break;
        }

        case Op::RETURN_0: {
            close_upvalues(regs);
            --m_frame_count;
            if (m_frame_count < entry_depth) return Value::none_val();
            regs[-1] = Value::none_val();
            frame = &m_frames[m_frame_count - 1];
            pc = frame->pc; regs = frame->regs; ck = frame->chunk;
            break;
        }
        case Op::RETURN_1: {
            uint8_t a = INS_A(w);
            Value ret = REGS[a];
            close_upvalues(regs);
            --m_frame_count;
            if (m_frame_count < entry_depth) return ret;
            regs[-1] = ret;
            frame = &m_frames[m_frame_count - 1];
            pc = frame->pc; regs = frame->regs; ck = frame->chunk;
            break;
        }
        case Op::RETURN: {
            uint8_t b = INS_B(w);
            Value ret = REGS[b];
            close_upvalues(regs);
            --m_frame_count;
            if (m_frame_count < entry_depth) return ret;
            regs[-1] = ret;
            frame = &m_frames[m_frame_count - 1];
            pc = frame->pc; regs = frame->regs; ck = frame->chunk;
            break;
        }

        // ── Type conversion ──────────────────────────────────────────────────
        case Op::TO_STR: {
            uint8_t a=INS_A(w),b=INS_B(w);
            REGS[a] = Value::from_ptr(alloc_string(val_to_string(REGS[b])));
            break;
        }
        case Op::TO_INT: {
            uint8_t a=INS_A(w),b=INS_B(w);
            Value v = REGS[b];
            if (v.is_int())   { REGS[a] = v; break; }
            if (v.is_float()) { REGS[a] = Value::from_int(int64_t(v.as_float())); break; }
            if (val_is_string(v)) {
                try { REGS[a] = Value::from_int(std::stoll(as_str(v).data)); break; }
                catch (...) {}
            }
            throw std::runtime_error("cannot convert to int");
        }
        case Op::TO_FLOAT: {
            uint8_t a=INS_A(w),b=INS_B(w);
            Value v = REGS[b];
            if (v.is_float()) { REGS[a] = v; break; }
            if (v.is_int())   { REGS[a] = Value::from_float(double(v.as_int())); break; }
            throw std::runtime_error("cannot convert to float");
        }
        case Op::TO_BOOL: {
            uint8_t a=INS_A(w),b=INS_B(w);
            REGS[a] = Value::from_bool(REGS[b].truthy());
            break;
        }
        case Op::TYPEOF: {
            uint8_t a=INS_A(w),b=INS_B(w);
            const char* t = "none";
            Value v = REGS[b];
            if (v.is_int() || v.is_float()) t = "number";
            else if (v.is_bool()) t = "bool";
            else if (val_is_string(v)) t = "string";
            else if (val_is_list(v)) t = "list";
            else if (val_is_map(v)) t = "map";
            else if (val_is_callable(v)) t = "function";
            REGS[a] = Value::from_ptr(alloc_string(t, strlen(t)));
            break;
        }

        // ── Null coalescing ──────────────────────────────────────────────────
        case Op::NULLC: {
            uint8_t a=INS_A(w),b=INS_B(w),c=INS_C(w);
            REGS[a] = REGS[b].is_none() ? REGS[c] : REGS[b];
            break;
        }

        // ── Error handling (minimal — rethrow as C++ exception for now) ──────
        case Op::THROW: {
            uint8_t a = INS_A(w);
            throw std::runtime_error(val_to_string(REGS[a]));
        }
        case Op::TRY_PUSH: case Op::TRY_POP: case Op::ERR_NEW:
            // ponytail: try/catch → TODO; just no-op for benchmarks
            break;

        case Op::NOP: break;

        case Op::HALT: {
            frame->pc = pc;
            return REGS[0];
        }

        default:
            throw std::runtime_error("unknown opcode");
        }
    }

#undef NEXT_INS
#undef REGS
}

Value VM::run(ObjFunction* fn)
{
    tls_vm = this;

    // Wrap the bare function in a closure (zero upvalues)
    auto* closure = alloc<ObjClosure>(fn);
    int base = 0;

    CallFrame& frame = m_frames[m_frame_count++];
    frame.closure = closure;
    frame.chunk   = fn->chunk;
    frame.pc      = fn->chunk->code.data();
    frame.regs    = m_stack + base;
    frame.base    = base;

    Value result = run_frame(frame);
    --m_frame_count;
    tls_vm = nullptr;
    return result;
}

} // namespace syn
