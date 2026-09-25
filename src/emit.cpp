// LLM Usage Level: Assistant

#include "../include/skald_emit.h"
#include "skald_version.h"

namespace Skald::Emit {

namespace {

/** Appends key unless the value is nil. */
void put(Table &t, const char *key, Value v) {
  if (!v.is_nil()) {
    t.emplace_back(key, std::move(v));
  }
}

Value simple(const SimpleRValue &val) {
  return std::visit(
      [](const auto &x) -> Value {
        using T = std::decay_t<decltype(x)>;
        Table t;
        if constexpr (std::is_same_v<T, std::string>) {
          put(t, "t", "str");
        } else if constexpr (std::is_same_v<T, bool>) {
          put(t, "t", "bool");
        } else if constexpr (std::is_same_v<T, int>) {
          put(t, "t", "int");
        } else {
          put(t, "t", "float");
        }
        put(t, "v", x);
        return t;
      },
      val);
}

Value rvalue(const RValue &val);

Value call(const MethodCallOp &op) {
  Table t;
  put(t, "t", "call");
  put(t, "method", op.method);
  Array args;
  for (auto &a : op.args) {
    args.push_back(rvalue(a));
  }
  put(t, "args", args);
  put(t, "line", op.line_number);
  return t;
}

Value rvalue(const RValue &val) {
  if (auto s = cast_rval_to_simple(val)) {
    return simple(*s);
  }
  if (auto *var = rval_get_var(val)) {
    Table t;
    put(t, "t", "var");
    put(t, "name", var->name);
    return t;
  }
  return call(*rval_get_call(val));
}

const char *cmp_name(ConditionalAtom::Comparison c) {
  switch (c) {
  case ConditionalAtom::TRUTHY:
    return "truthy";
  case ConditionalAtom::NOT_TRUTHY:
    return "not_truthy";
  case ConditionalAtom::EQUALS:
    return "eq";
  case ConditionalAtom::NOT_EQUALS:
    return "ne";
  case ConditionalAtom::MORE:
    return "gt";
  case ConditionalAtom::LESS:
    return "lt";
  case ConditionalAtom::MORE_EQUAL:
    return "ge";
  case ConditionalAtom::LESS_EQUAL:
    return "le";
  }
  return "truthy";
}

Value conditional(const Conditional &cond) {
  Table t;
  put(t, "type", cond.type == Conditional::AND ? "and" : "or");
  Array items;
  for (auto &item : cond.items) {
    if (auto *atom = std::get_if<ConditionalAtom>(&item)) {
      Table a;
      put(a, "t", "atom");
      put(a, "a", rvalue(atom->a));
      put(a, "cmp", cmp_name(atom->comparison));
      if (atom->b) {
        put(a, "b", rvalue(*atom->b));
      }
      items.push_back(a);
    } else {
      Table g;
      put(g, "t", "group");
      put(g, "cond",
          conditional(*std::get<std::shared_ptr<Conditional>>(item)));
      items.push_back(g);
    }
  }
  put(t, "items", items);
  return t;
}

Value attached(const AttachedCondition &ac) {
  if (!ac.condition) {
    return Value{};
  }
  return conditional(*ac.condition);
}

Value text(const TextContent &content) {
  Array parts;
  for (auto &part : content.parts) {
    if (auto *lit = std::get_if<std::string>(&part)) {
      parts.push_back(*lit);
    } else if (auto *ins = std::get_if<SimpleInsertion>(&part)) {
      Table t;
      put(t, "t", "insert");
      put(t, "rv", rvalue(ins->rvalue));
      parts.push_back(t);
    } else {
      auto &tern = std::get<TernaryInsertion>(part);
      Table t;
      put(t, "t", "tern");
      put(t, "check", rvalue(tern.check));
      put(t, "truthy", tern.check_truthy);
      Array options;
      for (auto &opt : tern.options) {
        options.push_back(
            Array{rvalue(std::get<0>(opt)), rvalue(std::get<1>(opt))});
      }
      put(t, "options", options);
      parts.push_back(t);
    }
  }
  Table t;
  put(t, "parts", parts);
  return t;
}

const char *mutation_op(Mutation::Type type) {
  switch (type) {
  case Mutation::EQUATE:
    return "set";
  case Mutation::ADD:
    return "add";
  case Mutation::SUBTRACT:
    return "sub";
  case Mutation::SWITCH:
    return "toggle";
  }
  return "set";
}

Value body(const MemberBody &b) {
  return std::visit(
      [](const auto &x) -> Value {
        using T = std::decay_t<decltype(x)>;
        Table t;
        if constexpr (std::is_same_v<T, Beat>) {
          put(t, "t", "beat");
          put(t, "attribution", x.attribution);
          put(t, "text", text(x.content));
        } else if constexpr (std::is_same_v<T, Move>) {
          put(t, "t", "move");
          put(t, "target", x.target_tag);
        } else if constexpr (std::is_same_v<T, MethodCallOp>) {
          put(t, "t", "call");
          put(t, "method", x.method);
          Array args;
          for (auto &a : x.args) {
            args.push_back(rvalue(a));
          }
          put(t, "args", args);
        } else if constexpr (std::is_same_v<T, Mutation>) {
          put(t, "t", "mutation");
          put(t, "op", mutation_op(x.type));
          put(t, "lvalue", x.lvalue);
          if (x.rvalue) {
            put(t, "rv", rvalue(*x.rvalue));
          }
        } else if constexpr (std::is_same_v<T, GoModule>) {
          put(t, "t", "go");
          put(t, "module", x.module_path);
          put(t, "start_in", x.start_in_tag);
        } else {
          put(t, "t", "exit");
          if (x.argument) {
            put(t, "arg", simple(*x.argument));
          }
        }
        return t;
      },
      b);
}

Value member(const Member &m) {
  Table t;
  put(t, "cond", attached(m.ac));
  put(t, "line", m.line_number);
  put(t, "body", body(m.body));
  return t;
}

Value choice_group(const ChoiceGroup &cg) {
  Table t;
  put(t, "t", "choices");
  put(t, "line", cg.line_number);
  Array choices;
  for (auto &c : cg.choices) {
    Table ct;
    put(ct, "cond", attached(c.condition));
    put(ct, "text", text(c.content));
    Array members;
    for (auto &m : c.members) {
      members.push_back(member(m));
    }
    put(ct, "members", members);
    put(ct, "line", c.line_number);
    choices.push_back(ct);
  }
  put(t, "choices", choices);
  return t;
}

Value block_member(const BlockMember &bm) {
  if (auto *m = std::get_if<Member>(&bm)) {
    return member(*m);
  }
  return choice_group(std::get<ChoiceGroup>(bm));
}

Value chain(const ConditionalChain &cc) {
  Table t;
  put(t, "t", "chain");
  Array blocks;
  for (auto &cb : cc.cond_blocks) {
    Table bt;
    put(bt, "cond", attached(cb.cond));
    put(bt, "line", cb.line_number);
    Array members;
    for (auto &m : cb.members) {
      members.push_back(block_member(m));
    }
    put(bt, "members", members);
    blocks.push_back(bt);
  }
  put(t, "blocks", blocks);
  return t;
}

Value main_member(const MainBlockMember &mbm) {
  if (auto *bm = std::get_if<BlockMember>(&mbm)) {
    return block_member(*bm);
  }
  return chain(std::get<ConditionalChain>(mbm));
}

Value declared_var(const DeclaredVar &var, bool with_line) {
  Table t;
  put(t, "name", var.var.name);
  put(t, "type", val_type_to_str(var.var.type));
  put(t, "default", simple(var.initial_value));
  if (with_line) {
    put(t, "line", var.line_number);
  }
  return t;
}

void put_header(Table &t) {
  put(t, "format_version", FORMAT_VERSION);
  put(t, "skald_version", SKALD_VERSION);
}

} // namespace

Value build_codex_tree(const Codex &codex,
                       const std::vector<std::string> &module_paths) {
  Table t;
  put_header(t);
  put(t, "name", codex.filename);
  Array globals;
  for (auto &g : codex.global_vars) {
    globals.push_back(declared_var(g, false));
  }
  put(t, "globals", globals);
  Array methods;
  for (auto &m : codex.method_defs) {
    Table mt;
    put(mt, "name", m.name);
    put(mt, "ret", val_type_to_str(m.return_type));
    Array args;
    for (auto &a : m.args) {
      Table at;
      put(at, "name", a.name);
      put(at, "type", val_type_to_str(a.type));
      args.push_back(at);
    }
    put(mt, "args", args);
    methods.push_back(mt);
  }
  put(t, "methods", methods);
  Array modules;
  for (auto &p : module_paths) {
    modules.push_back(p);
  }
  put(t, "modules", modules);
  return t;
}

Value build_module_tree(const Module &module, const std::string &path) {
  Table t;
  put_header(t);
  put(t, "path", path);
  Array vars;
  for (auto &v : module.module_vars) {
    vars.push_back(declared_var(v, true));
  }
  put(t, "vars", vars);
  Array testbeds;
  for (auto &tb : module.testbeds) {
    Table tt;
    put(tt, "name", tb.name);
    put(tt, "line", tb.line_number);
    Array sets;
    for (auto &s : tb.declarations) {
      Table st;
      put(st, "var", s.variable);
      put(st, "value", simple(s.test_value));
      put(st, "line", s.line_number);
      sets.push_back(st);
    }
    put(tt, "sets", sets);
    testbeds.push_back(tt);
  }
  put(t, "testbeds", testbeds);
  Array blocks;
  for (auto &b : module.blocks) {
    Table bt;
    put(bt, "tag", b.tag);
    put(bt, "line", b.line_number);
    Array members;
    for (auto &m : b.members) {
      members.push_back(main_member(m));
    }
    put(bt, "members", members);
    blocks.push_back(bt);
  }
  put(t, "blocks", blocks);
  return t;
}

} // namespace Skald::Emit
