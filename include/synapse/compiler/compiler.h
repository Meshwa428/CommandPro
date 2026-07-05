#pragma once
#include <functional>
#include <vector>
#include <string>
#include <unordered_map>
#include "synapse/common/source.h"
#include "synapse/common/diag.h"
#include "synapse/frontend/ast.h"
#include "synapse/runtime/value.h"
#include "synapse/runtime/chunk.h"

namespace syn {

struct ObjFunction;
struct VM;

// ── Compiler ──────────────────────────────────────────────────────────────────
// Single-pass AST → bytecode compiler.
// Folding in the resolver: locals are assigned slots as they are declared.

class Compiler {
public:
    // Compile a full program into a top-level ObjFunction.
    // Returns null on error.
    // module_mode=true: top-level fn/let/const also emit SET_GLOBAL so their
    // values persist after the __main__ frame exits (needed for use/import).
    static ObjFunction* compile(const Program& prog, const Source& src,
                                DiagEngine& diag, VM& vm,
                                bool module_mode = false);

private:
    // ── Local variable tracking ───────────────────────────────────────────────
    struct Local {
        std::string name;
        int         slot;       // register index
        int         depth;      // scope depth (-1 = declared but not initialized)
        bool        is_captured = false;
    };

    struct UpvalueInfo {
        bool is_local; // true = from enclosing fn's locals; false = from enclosing fn's upvalues
        int  index;
    };

    // Per-function compiler state
    struct FnState {
        ObjFunction*          fn;
        std::vector<Local>    locals;
        std::vector<UpvalueInfo> upvalues;
        int                   scope_depth = 0;
        int                   reg_top     = 0; // next free temporary register
        FnState*              enclosing   = nullptr;
    };

    // ── Patch lists for forward jumps ─────────────────────────────────────────
    struct PatchList {
        std::vector<std::size_t> jumps;
    };

    Compiler(const Source& src, DiagEngine& diag, VM& vm);

    ObjFunction* compile_program(const Program& prog);

    // ── Function compilation ─────────────────────────────────────────────────
    // out_upvalues receives the inner fn's capture list so caller can emit
    // the per-upvalue descriptor instructions right after CLOSURE.
    // params (when given) drives the default-value preamble and *rest flag.
    ObjFunction* compile_function(const std::string& name, int arity,
                                  const std::vector<std::string>& param_names,
                                  const Block& body,
                                  std::vector<UpvalueInfo>* out_upvalues = nullptr,
                                  const std::vector<Param>* params = nullptr);

    // ── Statement compilation ─────────────────────────────────────────────────
    void compile_stmt(const StmtNode* stmt);
    void compile_let(const LetStmt* stmt);
    void compile_const(const ConstStmt* stmt);
    void compile_assign(const AssignStmt* stmt);
    void compile_aug_assign(const AugAssignStmt* stmt);
    void compile_if(const IfStmt* stmt);
    void compile_while(const WhileStmt* stmt);
    void compile_repeat(const RepeatStmt* stmt);
    void compile_for(const ForStmt* stmt);
    void compile_return(const ReturnStmt* stmt);
    void compile_break();
    void compile_continue();
    void compile_fn_decl(const FnDeclStmt* stmt);
    void compile_try(const TryStmt* stmt);
    void compile_throw(const ThrowStmt* stmt);
    void compile_expr_stmt(const ExprStmt* stmt);
    void compile_block(const Block& stmts);

    // ── Expression compilation (returns the register holding the result) ──────
    int  compile_expr(const ExprNode* expr, int dest = -1);
    int  compile_int_lit(const IntLitExpr* e, int dest);
    int  compile_float_lit(const FloatLitExpr* e, int dest);
    int  compile_string_lit(const StringLitExpr* e, int dest);
    int  compile_interp_string(const InterpStringExpr* e, int dest);
    int  compile_ident(const IdentExpr* e, int dest);
    int  compile_binary(const BinaryExpr* e, int dest);
    int  compile_unary(const UnaryExpr* e, int dest);
    int  compile_ternary(const TernaryExpr* e, int dest);
    int  compile_call(const CallExpr* e, int dest);
    int  compile_index(const IndexExpr* e, int dest);
    int  compile_field(const FieldExpr* e, int dest);
    int  compile_list(const ListExpr* e, int dest);
    int  compile_map(const MapExpr* e, int dest);
    int  compile_tuple(const TupleExpr* e, int dest);
    int  compile_fn_expr(const FnExpr* e, int dest);
    int  compile_match_expr(const MatchExpr* e, int dest);
    int  compile_pipe(const BinaryExpr* e, int dest);
    int  compile_slice(const SliceExpr* e, int dest);
    void compile_pattern_match(const Pattern& pat, int subj, PatchList& fail);
    int  compile_list_comp(const ListCompExpr* e, int dest);
    int  compile_map_comp(const MapCompExpr* e, int dest);
    // Emits the nested comprehension loops; `body` emits the innermost code.
    void compile_comp_level(const std::vector<const CompFor*>& fors, std::size_t level,
                            const std::function<void()>& body);

    // ── Register management ───────────────────────────────────────────────────
    int  alloc_reg();           // allocate a temporary register
    void free_reg(int n = 1);   // release n temporaries
    int  reg_top() const;       // current top
    // Compile a read-only source operand. For a local variable, returns the
    // variable's own register (no MOVE, no temp). Otherwise allocates a temp at
    // the reg-stack top and compiles into it. Sets is_temp so the caller knows
    // whether to free_reg() (frees must still happen in LIFO order).
    int  compile_operand(const ExprNode* e, bool& is_temp);

    // ── Scope management ─────────────────────────────────────────────────────
    void begin_scope();
    void end_scope();
    int  declare_local(const std::string& name);
    int  resolve_local(const std::string& name) const;
    int  resolve_upvalue(const std::string& name);
    int  add_upvalue(bool is_local, int idx);
    void mark_initialized(int slot);

    // ── Emit helpers ──────────────────────────────────────────────────────────
    Chunk&      chunk();
    void        emit(uint64_t instr, uint32_t line = 0);
    std::size_t emit_jump(uint64_t instr, uint32_t line = 0);
    void        patch_jump(std::size_t jmp_idx);
    void        patch_list(PatchList& pl);
    uint16_t    add_const(Value v);
    uint16_t    add_str_const(const std::string& s);
    void        move_to(int src, int dst);
    int         ensure_reg(int result, int dest);

    // ── Lvalue helpers ───────────────────────────────────────────────────────
    void assign_to(const ExprNode* lval, int src_reg);

    // ── Diagnostics ──────────────────────────────────────────────────────────
    // Emit a compile error for a grammar feature the parser accepts but codegen
    // doesn't implement yet. Never drop a construct silently.
    void unsupported(Span span, const std::string& what);

    // ── Loop/break/continue stack ─────────────────────────────────────────────
    struct LoopInfo {
        PatchList breaks;
        PatchList continues;
        std::size_t continue_target; // pc of the loop's RANGE_STEP / condition check
    };
    std::vector<LoopInfo> m_loop_stack;

    // ── State ─────────────────────────────────────────────────────────────────
    const Source& m_source;
    DiagEngine&   m_diag;
    VM&           m_vm;
    FnState*      m_current = nullptr;
    bool          m_module_mode = false;

    // Line of the statement currently being compiled — set once per
    // compile_stmt() call, used by emit() as the default for runtime error
    // reporting (statement-level granularity, not per-expression).
    uint32_t      m_current_line = 0;
};

} // namespace syn
