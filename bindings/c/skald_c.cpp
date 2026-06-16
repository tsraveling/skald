// skald_c.cpp - C interface implementation
// Compile with: c++ -std=c++17 -fPIC -shared skald_c.cpp [other skald sources]
// -o libskald.so

#include "skald_c.h"
#include "skald.h"
#include "skald_version.h"
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// Internal struct definitions (opaque to C consumers)
// -----------------------------------------------------------------------------

struct SkaldEngine {
  Skald::Engine engine;

  // Holds the value from the most recent successful skald_engine_get_global so
  // the typed accessors return stable pointers / values after the call.
  Skald::SimpleRValue last_global = std::string();
  std::string last_global_string;
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
  std::string notif_string_cache;
};

// Pull the MethodCall out of either method-call response variant.
static const Skald::MethodCall *get_method_call(const SkaldResponse *response) {
  if (auto *g = std::get_if<Skald::MethodCallGet>(&response->response))
    return &g->call;
  if (auto *p = std::get_if<Skald::MethodCallPost>(&response->response))
    return &p->call;
  return nullptr;
}

// -----------------------------------------------------------------------------
// Helper: wrap a Skald::Response into a SkaldResponse with cached strings
// -----------------------------------------------------------------------------

static SkaldResponse *wrap_response(Skald::Response resp) {
  auto *r = new SkaldResponse{std::move(resp), {}, {}, {}, {}, {}};

  // Pre-cache strings based on response type
  if (auto *content = std::get_if<Skald::Content>(&r->response)) {
    // Concatenate all chunks into single text
    for (const auto &chunk : content->text) {
      r->text_cache += chunk.text;
    }
  } else if (auto *group = std::get_if<Skald::OptionGroup>(&r->response)) {
    // Cache option texts (options are their own response type in 0.3)
    for (const auto &opt : group->options) {
      std::string opt_text;
      for (const auto &chunk : opt.text) {
        opt_text += chunk.text;
      }
      r->option_texts.push_back(std::move(opt_text));
    }
  } else if (auto *call = get_method_call(r)) {
    // Convert all args to strings (covers both GET and POST calls)
    for (const auto &arg : call->args) {
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
  } else if (auto *notif = std::get_if<Skald::Notification>(&r->response)) {
    // Cache notification value if it's a string
    if (notif->rval) {
      if (auto *s = Skald::srval_get_str(*notif->rval)) {
        r->notif_string_cache = *s;
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

SkaldErrorCode skald_engine_load(SkaldEngine *engine, const char *path) {
  if (!engine || !path)
    return SKALD_ERR_UNEXPECTED_NULL;
  Skald::ParseResult result = engine->engine.load(path);
  return result.ok ? SKALD_OK : SKALD_ERR_LOADING_MODULE;
}

// -----------------------------------------------------------------------------
// Global State Access
// -----------------------------------------------------------------------------

// Map the optional<Error> returned by Engine::set into a C error code.
static SkaldErrorCode set_result(std::optional<Skald::Error> err) {
  if (!err)
    return SKALD_OK;
  return static_cast<SkaldErrorCode>(err->code);
}

SkaldErrorCode skald_engine_set_global_string(SkaldEngine *engine,
                                              const char *key,
                                              const char *value) {
  if (!engine || !key)
    return SKALD_ERR_VAR_UNDEFINED;
  return set_result(
      engine->engine.set(key, Skald::SimpleRValue(std::string(value ? value : ""))));
}

SkaldErrorCode skald_engine_set_global_bool(SkaldEngine *engine,
                                            const char *key, bool value) {
  if (!engine || !key)
    return SKALD_ERR_VAR_UNDEFINED;
  return set_result(engine->engine.set(key, Skald::SimpleRValue(value)));
}

SkaldErrorCode skald_engine_set_global_int(SkaldEngine *engine, const char *key,
                                           int value) {
  if (!engine || !key)
    return SKALD_ERR_VAR_UNDEFINED;
  return set_result(engine->engine.set(key, Skald::SimpleRValue(value)));
}

SkaldErrorCode skald_engine_set_global_float(SkaldEngine *engine,
                                             const char *key, float value) {
  if (!engine || !key)
    return SKALD_ERR_VAR_UNDEFINED;
  return set_result(engine->engine.set(key, Skald::SimpleRValue(value)));
}

bool skald_engine_get_global(SkaldEngine *engine, const char *key,
                             SkaldValueType *out_type) {
  if (!engine || !key || !out_type)
    return false;
  auto result = engine->engine.get(key);
  if (std::get_if<Skald::Error>(&result))
    return false;
  engine->last_global = std::get<Skald::SimpleRValue>(result);
  *out_type =
      static_cast<SkaldValueType>(Skald::srval_get_type(engine->last_global));
  if (auto *s = Skald::srval_get_str(engine->last_global)) {
    engine->last_global_string = *s;
  } else {
    engine->last_global_string.clear();
  }
  return true;
}

const char *skald_engine_get_global_string(SkaldEngine *engine) {
  if (!engine)
    return "";
  return engine->last_global_string.c_str();
}

bool skald_engine_get_global_bool(SkaldEngine *engine) {
  if (!engine)
    return false;
  if (auto *b = Skald::srval_get_bool(engine->last_global))
    return *b;
  return false;
}

int skald_engine_get_global_int(SkaldEngine *engine) {
  if (!engine)
    return 0;
  if (auto *i = Skald::srval_get_int(engine->last_global))
    return *i;
  return 0;
}

float skald_engine_get_global_float(SkaldEngine *engine) {
  if (!engine)
    return 0.0f;
  if (auto *f = Skald::srval_get_float(engine->last_global))
    return *f;
  return 0.0f;
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

// -----------------------------------------------------------------------------
// OptionGroup Accessors
// -----------------------------------------------------------------------------

size_t skald_option_group_get_count(const SkaldResponse *response) {
  if (!response)
    return 0;
  if (auto *g = std::get_if<Skald::OptionGroup>(&response->response)) {
    return g->options.size();
  }
  return 0;
}

const char *skald_option_group_get_text(const SkaldResponse *response,
                                        size_t index) {
  if (!response || index >= response->option_texts.size())
    return "";
  return response->option_texts[index].c_str();
}

bool skald_option_group_get_available(const SkaldResponse *response,
                                      size_t index) {
  if (!response)
    return false;
  if (auto *g = std::get_if<Skald::OptionGroup>(&response->response)) {
    if (index < g->options.size()) {
      return g->options[index].is_available;
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
  if (auto *call = get_method_call(response)) {
    return call->method.c_str();
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
  // A GET call keys a return value back into state; a POST call does not.
  return std::holds_alternative<Skald::MethodCallGet>(response->response);
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

// Unwrap an Exit response's argument into a SimpleRValue, or nullopt if this is
// not an Exit, carries no argument, or the argument is not simple-castable.
static std::optional<Skald::SimpleRValue>
exit_simple(const SkaldResponse *response) {
  if (!response)
    return std::nullopt;
  if (auto *e = std::get_if<Skald::Exit>(&response->response)) {
    if (e->argument)
      return Skald::cast_rval_to_simple(*e->argument);
  }
  return std::nullopt;
}

// Borrow the SimpleRValue carried by a Notification response, or nullptr if
// this is not a Notification or it has no value. The pointer is valid for the
// lifetime of the response.
static const Skald::SimpleRValue *notif_simple(const SkaldResponse *response) {
  if (!response)
    return nullptr;
  if (auto *n = std::get_if<Skald::Notification>(&response->response)) {
    if (n->rval)
      return &*n->rval;
  }
  return nullptr;
}

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
  if (auto simple = exit_simple(response)) {
    if (auto *i = Skald::srval_get_int(*simple))
      return *i;
  }
  return 0;
}

bool skald_exit_get_bool(const SkaldResponse *response) {
  if (auto simple = exit_simple(response)) {
    if (auto *b = Skald::srval_get_bool(*simple))
      return *b;
  }
  return false;
}

float skald_exit_get_float(const SkaldResponse *response) {
  if (auto simple = exit_simple(response)) {
    if (auto *f = Skald::srval_get_float(*simple))
      return *f;
  }
  return 0.0f;
}

SkaldValueType skald_exit_get_type(const SkaldResponse *response) {
  if (auto simple = exit_simple(response))
    return static_cast<SkaldValueType>(Skald::srval_get_type(*simple));
  return SKALD_VALUE_STRING;
}

// -----------------------------------------------------------------------------
// Notification Accessors
// -----------------------------------------------------------------------------

const char *skald_notification_get_var_name(const SkaldResponse *response) {
  if (!response)
    return "";
  if (auto *n = std::get_if<Skald::Notification>(&response->response)) {
    return n->var_name.c_str();
  }
  return "";
}

bool skald_notification_has_value(const SkaldResponse *response) {
  if (!response)
    return false;
  if (auto *n = std::get_if<Skald::Notification>(&response->response)) {
    return n->rval.has_value();
  }
  return false;
}

SkaldValueType skald_notification_get_type(const SkaldResponse *response) {
  if (auto *rval = notif_simple(response))
    return static_cast<SkaldValueType>(Skald::srval_get_type(*rval));
  return SKALD_VALUE_STRING;
}

const char *skald_notification_get_string(const SkaldResponse *response) {
  if (!response)
    return "";
  return response->notif_string_cache.c_str();
}

bool skald_notification_get_bool(const SkaldResponse *response) {
  if (auto *rval = notif_simple(response)) {
    if (auto *b = Skald::srval_get_bool(*rval))
      return *b;
  }
  return false;
}

int skald_notification_get_int(const SkaldResponse *response) {
  if (auto *rval = notif_simple(response)) {
    if (auto *i = Skald::srval_get_int(*rval))
      return *i;
  }
  return 0;
}

float skald_notification_get_float(const SkaldResponse *response) {
  if (auto *rval = notif_simple(response)) {
    if (auto *f = Skald::srval_get_float(*rval))
      return *f;
  }
  return 0.0f;
}

// -----------------------------------------------------------------------------
// Version Info
// -----------------------------------------------------------------------------

const char *skald_version(void) { return SKALD_VERSION; }

int skald_version_major(void) { return SKALD_VERSION_MAJOR; }

int skald_version_minor(void) { return SKALD_VERSION_MINOR; }

int skald_version_patch(void) { return SKALD_VERSION_PATCH; }

} // extern "C"
