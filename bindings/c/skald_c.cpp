// skald_c.cpp - C interface implementation
// Compile with: c++ -std=c++17 -fPIC -shared skald_c.cpp [other skald sources]
// -o libskald.so

#include "skald_c.h"
#include "skald.h"
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// Internal struct definitions (opaque to C consumers)
// -----------------------------------------------------------------------------

struct SkaldEngine {
  Skald::Engine engine;
};

// SkaldResponse holds the C++ response plus cached string conversions.
// We cache strings because C code needs stable pointers that outlive function
// calls.
struct SkaldResponse {
  Skald::Response response;

  // Cached conversions for strings we return pointers to
  std::string text_cache;
  std::vector<std::string> option_texts;
  std::vector<std::string> query_args;
  std::string exit_string_cache;
};

// -----------------------------------------------------------------------------
// Helper: wrap a Skald::Response into a SkaldResponse with cached strings
// -----------------------------------------------------------------------------

static SkaldResponse *wrap_response(Skald::Response resp) {
  auto *r = new SkaldResponse{std::move(resp), {}, {}, {}, {}};

  // Pre-cache strings based on response type
  if (auto *content = std::get_if<Skald::Content>(&r->response)) {
    // Concatenate all chunks into single text
    for (const auto &chunk : content->text) {
      r->text_cache += chunk.text;
    }
    // Cache option texts
    for (const auto &opt : content->options) {
      std::string opt_text;
      for (const auto &chunk : opt.text) {
        opt_text += chunk.text;
      }
      r->option_texts.push_back(std::move(opt_text));
    }
  } else if (auto *query = std::get_if<Skald::Query>(&r->response)) {
    // Convert all args to strings
    for (const auto &arg : query->call.args) {
      r->query_args.push_back(Skald::rval_to_string(arg));
    }
  } else if (auto *exit = std::get_if<Skald::Exit>(&r->response)) {
    // Cache exit value if it's a string
    if (exit->argument) {
      if (auto simple = Skald::cast_rval_to_simple(*exit->argument)) {
        if (auto *s = Skald::srval_get_str(*simple)) {
          r->exit_string_cache = *s;
        }
      }
    }
  }

  return r;
}

// -----------------------------------------------------------------------------
// Engine Lifecycle
// -----------------------------------------------------------------------------

extern "C" {

SkaldEngine *skald_engine_new(void) { return new (std::nothrow) SkaldEngine; }

void skald_engine_free(SkaldEngine *engine) { delete engine; }

void skald_engine_load(SkaldEngine *engine, const char *path) {
  if (engine && path) {
    engine->engine.load(path);
  }
}

// -----------------------------------------------------------------------------
// Engine Actions
// -----------------------------------------------------------------------------

SkaldResponse *skald_engine_start(SkaldEngine *engine) {
  if (!engine)
    return nullptr;
  return wrap_response(engine->engine.start());
}

SkaldResponse *skald_engine_start_at(SkaldEngine *engine, const char *tag) {
  if (!engine || !tag)
    return nullptr;
  return wrap_response(engine->engine.start_at(tag));
}

SkaldResponse *skald_engine_act(SkaldEngine *engine, int choice_index) {
  if (!engine)
    return nullptr;
  return wrap_response(engine->engine.act(choice_index));
}

SkaldResponse *skald_engine_answer_string(SkaldEngine *engine,
                                          const char *value) {
  if (!engine)
    return nullptr;
  Skald::QueryAnswer answer;
  answer.val = std::string(value ? value : "");
  return wrap_response(engine->engine.answer(answer));
}

SkaldResponse *skald_engine_answer_bool(SkaldEngine *engine, bool value) {
  if (!engine)
    return nullptr;
  Skald::QueryAnswer answer;
  answer.val = value;
  return wrap_response(engine->engine.answer(answer));
}

SkaldResponse *skald_engine_answer_int(SkaldEngine *engine, int value) {
  if (!engine)
    return nullptr;
  Skald::QueryAnswer answer;
  answer.val = value;
  return wrap_response(engine->engine.answer(answer));
}

SkaldResponse *skald_engine_answer_float(SkaldEngine *engine, float value) {
  if (!engine)
    return nullptr;
  Skald::QueryAnswer answer;
  answer.val = value;
  return wrap_response(engine->engine.answer(answer));
}

SkaldResponse *skald_engine_answer_null(SkaldEngine *engine) {
  if (!engine)
    return nullptr;
  return wrap_response(engine->engine.answer(std::nullopt));
}

// -----------------------------------------------------------------------------
// Response Inspection
// -----------------------------------------------------------------------------

SkaldResponseType skald_response_type(const SkaldResponse *response) {
  if (!response)
    return SKALD_RESPONSE_ERROR;
  return static_cast<SkaldResponseType>(response->response.index());
}

void skald_response_free(SkaldResponse *response) { delete response; }

// -----------------------------------------------------------------------------
// Content Accessors
// -----------------------------------------------------------------------------

const char *skald_content_get_attribution(const SkaldResponse *response) {
  if (!response)
    return "";
  if (auto *c = std::get_if<Skald::Content>(&response->response)) {
    return c->attribution.c_str();
  }
  return "";
}

const char *skald_content_get_text(const SkaldResponse *response) {
  if (!response)
    return "";
  return response->text_cache.c_str();
}

size_t skald_content_get_option_count(const SkaldResponse *response) {
  if (!response)
    return 0;
  if (auto *c = std::get_if<Skald::Content>(&response->response)) {
    return c->options.size();
  }
  return 0;
}

const char *skald_content_get_option_text(const SkaldResponse *response,
                                          size_t index) {
  if (!response || index >= response->option_texts.size())
    return "";
  return response->option_texts[index].c_str();
}

bool skald_content_get_option_available(const SkaldResponse *response,
                                        size_t index) {
  if (!response)
    return false;
  if (auto *c = std::get_if<Skald::Content>(&response->response)) {
    if (index < c->options.size()) {
      return c->options[index].is_available;
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
// Query Accessors
// -----------------------------------------------------------------------------

const char *skald_query_get_method(const SkaldResponse *response) {
  if (!response)
    return "";
  if (auto *q = std::get_if<Skald::Query>(&response->response)) {
    return q->call.method.c_str();
  }
  return "";
}

size_t skald_query_get_arg_count(const SkaldResponse *response) {
  if (!response)
    return 0;
  return response->query_args.size();
}

const char *skald_query_get_arg(const SkaldResponse *response, size_t index) {
  if (!response || index >= response->query_args.size())
    return "";
  return response->query_args[index].c_str();
}

bool skald_query_expects_response(const SkaldResponse *response) {
  if (!response)
    return false;
  if (auto *q = std::get_if<Skald::Query>(&response->response)) {
    return q->expects_response;
  }
  return false;
}

// -----------------------------------------------------------------------------
// Error Accessors
// -----------------------------------------------------------------------------

SkaldErrorCode skald_error_get_code(const SkaldResponse *response) {
  if (!response)
    return SKALD_ERR_UNKNOWN;
  if (auto *e = std::get_if<Skald::Error>(&response->response)) {
    return static_cast<SkaldErrorCode>(e->code);
  }
  return SKALD_ERR_UNKNOWN;
}

const char *skald_error_get_message(const SkaldResponse *response) {
  if (!response)
    return "";
  if (auto *e = std::get_if<Skald::Error>(&response->response)) {
    return e->message.c_str();
  }
  return "";
}

size_t skald_error_get_line(const SkaldResponse *response) {
  if (!response)
    return 0;
  if (auto *e = std::get_if<Skald::Error>(&response->response)) {
    return e->line_number;
  }
  return 0;
}

// -----------------------------------------------------------------------------
// GoModule Accessors
// -----------------------------------------------------------------------------

const char *skald_go_module_get_path(const SkaldResponse *response) {
  if (!response)
    return "";
  if (auto *g = std::get_if<Skald::GoModule>(&response->response)) {
    return g->module_path.c_str();
  }
  return "";
}

const char *skald_go_module_get_tag(const SkaldResponse *response) {
  if (!response)
    return "";
  if (auto *g = std::get_if<Skald::GoModule>(&response->response)) {
    return g->start_in_tag.c_str();
  }
  return "";
}

// -----------------------------------------------------------------------------
// Exit Accessors
// -----------------------------------------------------------------------------

bool skald_exit_has_value(const SkaldResponse *response) {
  if (!response)
    return false;
  if (auto *e = std::get_if<Skald::Exit>(&response->response)) {
    return e->argument.has_value();
  }
  return false;
}

const char *skald_exit_get_string(const SkaldResponse *response) {
  if (!response)
    return "";
  return response->exit_string_cache.c_str();
}

int skald_exit_get_int(const SkaldResponse *response) {
  if (!response)
    return 0;
  if (auto *e = std::get_if<Skald::Exit>(&response->response)) {
    if (e->argument) {
      if (auto simple = Skald::cast_rval_to_simple(*e->argument)) {
        if (auto *i = Skald::srval_get_int(*simple)) {
          return *i;
        }
      }
    }
  }
  return 0;
}

} // extern "C"
