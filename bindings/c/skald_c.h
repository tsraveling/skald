// skald_c.h - C interface for Skald Engine
// This file is pure C and can be consumed by any FFI system.
//
// Build: compile skald_c.cpp with your skald sources into a shared library
//   - libskald.so (Linux)
//   - libskald.dylib (macOS)
//   - skald.dll (Windows)

#ifndef SKALD_C_H
#define SKALD_C_H

// Allow inclusion from C++
#ifdef __cplusplus
extern "C" {
#endif

// Platform-specific export macros for shared library builds
#ifdef SKALD_SHARED
#ifdef _WIN32
#ifdef SKALD_BUILD
#define SKALD_API __declspec(dllexport)
#else
#define SKALD_API __declspec(dllimport)
#endif
#else
#define SKALD_API __attribute__((visibility("default")))
#endif
#else
#define SKALD_API
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Opaque handles - the actual structs are defined in the .cpp file
typedef struct SkaldEngine SkaldEngine;
typedef struct SkaldResponse SkaldResponse;

// Response type enum - matches the order of Skald::Response in skald.h:
// std::variant<Content, MethodCallGet, MethodCallPost, Exit, GoModule,
//              OptionGroup, End, Error, Notification>
//
// METHOD_CALL_GET expects a return value (answer it); METHOD_CALL_POST is an
// action call with no return (still answer to advance, value ignored).
typedef enum SkaldResponseType {
  SKALD_RESPONSE_CONTENT = 0,
  SKALD_RESPONSE_METHOD_CALL_GET = 1,
  SKALD_RESPONSE_METHOD_CALL_POST = 2,
  SKALD_RESPONSE_EXIT = 3,
  SKALD_RESPONSE_GO_MODULE = 4,
  SKALD_RESPONSE_OPTION_GROUP = 5,
  SKALD_RESPONSE_END = 6,
  SKALD_RESPONSE_ERROR = 7,
  SKALD_RESPONSE_NOTIFICATION = 8
} SkaldResponseType;

// Error codes - match the constants in skald.hpp. SKALD_OK is C-bridge only
// (no Skald counterpart) and signals success from functions that return a code.
typedef enum SkaldErrorCode {
  SKALD_OK = -1,
  SKALD_ERR_UNKNOWN = 0,
  SKALD_ERR_EOF = 1,
  SKALD_ERR_EMPTY_MODULE = 2,
  SKALD_ERR_MODULE_TAG_NOT_FOUND = 3,
  SKALD_ERR_CHOICE_OUT_OF_BOUNDS = 4,
  SKALD_ERR_CHOICE_UNAVAILABLE = 5,
  SKALD_ERR_EXPECTED_ANSWER = 6,
  SKALD_ERR_RESOLUTION_QUEUE_EMPTY = 7,
  SKALD_ERR_TYPE_MISMATCH = 8,
  SKALD_ERR_UNEXPECTED_NULL = 9,
  SKALD_ERR_VAR_UNDEFINED = 10,
  SKALD_ERR_UNEXPECTED_ACT = 11,
  SKALD_ERR_LOADING_MODULE = 12,
  SKALD_ERR_NO_GLOBAL = 13
} SkaldErrorCode;

// Value type tag - matches Skald::ValueType ordering in skald.h
typedef enum SkaldValueType {
  SKALD_VALUE_STRING = 0,
  SKALD_VALUE_BOOL = 1,
  SKALD_VALUE_INT = 2,
  SKALD_VALUE_FLOAT = 3,
  SKALD_VALUE_ACTION = 4
} SkaldValueType;

// =============================================================================
// Engine Lifecycle
// =============================================================================

// Create a new engine instance. Returns NULL on allocation failure.
SKALD_API SkaldEngine *skald_engine_new(void);

// Destroy an engine and free all associated memory.
SKALD_API void skald_engine_free(SkaldEngine *engine);

// Load a module from a file path. Returns SKALD_OK on success, or an error
// code on failure (SKALD_ERR_UNEXPECTED_NULL for a null engine/path,
// SKALD_ERR_LOADING_MODULE if the module fails to parse). On failure the
// engine has no usable module loaded, so do not call skald_engine_start.
SKALD_API SkaldErrorCode skald_engine_load(SkaldEngine *engine,
                                           const char *path);

// =============================================================================
// Global State Access
//
// Globals are defined in the codex. Their lifecycle is the lifetime of the
// engine instance and they persist across module loads. Setting a global that
// does not exist, or with a mismatched type, fails.
// =============================================================================

// Set a global by type. Returns SKALD_OK on success, otherwise the error code
// (SKALD_ERR_VAR_UNDEFINED or SKALD_ERR_TYPE_MISMATCH).
SKALD_API SkaldErrorCode skald_engine_set_global_string(SkaldEngine *engine,
                                                        const char *key,
                                                        const char *value);
SKALD_API SkaldErrorCode skald_engine_set_global_bool(SkaldEngine *engine,
                                                      const char *key,
                                                      bool value);
SKALD_API SkaldErrorCode skald_engine_set_global_int(SkaldEngine *engine,
                                                     const char *key, int value);
SKALD_API SkaldErrorCode skald_engine_set_global_float(SkaldEngine *engine,
                                                       const char *key,
                                                       float value);

// Resolve a global. Returns true if the key exists, writing its type to
// out_type; on false the out-param is untouched. Read the resolved value with
// the typed accessors below, which reflect the most recent successful get.
SKALD_API bool skald_engine_get_global(SkaldEngine *engine, const char *key,
                                       SkaldValueType *out_type);
SKALD_API const char *skald_engine_get_global_string(SkaldEngine *engine);
SKALD_API bool skald_engine_get_global_bool(SkaldEngine *engine);
SKALD_API int skald_engine_get_global_int(SkaldEngine *engine);
SKALD_API float skald_engine_get_global_float(SkaldEngine *engine);

// =============================================================================
// Engine Actions - All return a SkaldResponse* that the caller must free
// =============================================================================

// Start execution at the first block in the loaded module.
SKALD_API SkaldResponse *skald_engine_start(SkaldEngine *engine);

// Start execution at a specific tag.
SKALD_API SkaldResponse *skald_engine_start_at(SkaldEngine *engine,
                                               const char *tag);

// Respond to Content with a choice selection (or 0 to continue).
SKALD_API SkaldResponse *skald_engine_act(SkaldEngine *engine,
                                          int choice_index);

// Answer a Query with various value types
SKALD_API SkaldResponse *skald_engine_answer_string(SkaldEngine *engine,
                                                    const char *value);
SKALD_API SkaldResponse *skald_engine_answer_bool(SkaldEngine *engine,
                                                  bool value);
SKALD_API SkaldResponse *skald_engine_answer_int(SkaldEngine *engine,
                                                 int value);
SKALD_API SkaldResponse *skald_engine_answer_float(SkaldEngine *engine,
                                                   float value);
SKALD_API SkaldResponse *skald_engine_answer_null(SkaldEngine *engine);

// =============================================================================
// Response Inspection
// =============================================================================

// Get the type of a response.
SKALD_API SkaldResponseType skald_response_type(const SkaldResponse *response);

// Free a response and all associated memory.
SKALD_API void skald_response_free(SkaldResponse *response);

// -----------------------------------------------------------------------------
// Content Response Accessors (only valid when type == SKALD_RESPONSE_CONTENT)
// -----------------------------------------------------------------------------

// Get the speaker attribution (may be empty string).
SKALD_API const char *
skald_content_get_attribution(const SkaldResponse *response);

// Get the full text content (all chunks concatenated).
SKALD_API const char *skald_content_get_text(const SkaldResponse *response);

// -----------------------------------------------------------------------------
// OptionGroup Response Accessors (only valid when type ==
// SKALD_RESPONSE_OPTION_GROUP)
//
// In 0.3 choices arrive as their own OptionGroup response, separate from
// Content. Respond by selecting an index with skald_engine_act().
// -----------------------------------------------------------------------------

// Get the number of choices in the group.
SKALD_API size_t skald_option_group_get_count(const SkaldResponse *response);

// Get the text of a specific option.
SKALD_API const char *
skald_option_group_get_text(const SkaldResponse *response, size_t index);

// Check if a specific option is currently available (not grayed out).
SKALD_API bool skald_option_group_get_available(const SkaldResponse *response,
                                                size_t index);

// -----------------------------------------------------------------------------
// Method Call Response Accessors (valid when type ==
// SKALD_RESPONSE_METHOD_CALL_GET or SKALD_RESPONSE_METHOD_CALL_POST)
//
// A GET call expects a return value; answer it with skald_engine_answer_*.
// A POST call is a fire-and-forget action (void): it is NOT pushed onto the
// resolution stack, so do not answer it. Advance past it with
// skald_engine_act() (any index) — calling skald_engine_answer_* on a POST
// returns SKALD_ERR_RESOLUTION_QUEUE_EMPTY.
// -----------------------------------------------------------------------------

// Get the method name being called.
SKALD_API const char *skald_query_get_method(const SkaldResponse *response);

// Get the number of arguments.
SKALD_API size_t skald_query_get_arg_count(const SkaldResponse *response);

// Get an argument as a string (all args are converted to string
// representation).
SKALD_API const char *skald_query_get_arg(const SkaldResponse *response,
                                          size_t index);

// Check if this call expects a return value (true for GET, false for POST).
SKALD_API bool skald_query_expects_response(const SkaldResponse *response);

// -----------------------------------------------------------------------------
// Error Response Accessors (only valid when type == SKALD_RESPONSE_ERROR)
// -----------------------------------------------------------------------------

// Get the error code.
SKALD_API SkaldErrorCode skald_error_get_code(const SkaldResponse *response);

// Get the error message.
SKALD_API const char *skald_error_get_message(const SkaldResponse *response);

// Get the line number where the error occurred.
SKALD_API size_t skald_error_get_line(const SkaldResponse *response);

// -----------------------------------------------------------------------------
// GoModule Response Accessors (only valid when type ==
// SKALD_RESPONSE_GO_MODULE)
// -----------------------------------------------------------------------------

// Get the module path to load.
SKALD_API const char *skald_go_module_get_path(const SkaldResponse *response);

// Get the tag to start at in the new module (may be empty).
SKALD_API const char *skald_go_module_get_tag(const SkaldResponse *response);

// -----------------------------------------------------------------------------
// Exit Response Accessors (only valid when type == SKALD_RESPONSE_EXIT)
// -----------------------------------------------------------------------------

// Check if the exit has a return value.
SKALD_API bool skald_exit_has_value(const SkaldResponse *response);

// Get exit value as string (returns empty string if no value or not a string).
SKALD_API const char *skald_exit_get_string(const SkaldResponse *response);

// Get exit value as int (returns 0 if no value or not an int).
SKALD_API int skald_exit_get_int(const SkaldResponse *response);

// Get exit value as bool (returns false if no value or not a bool).
SKALD_API bool skald_exit_get_bool(const SkaldResponse *response);

// Get exit value as float (returns 0.0 if no value or not a float).
SKALD_API float skald_exit_get_float(const SkaldResponse *response);

// Get the value type of the exit argument (only meaningful when
// skald_exit_has_value() is true).
SKALD_API SkaldValueType skald_exit_get_type(const SkaldResponse *response);

// -----------------------------------------------------------------------------
// Notification Response Accessors (only valid when type ==
// SKALD_RESPONSE_NOTIFICATION)
//
// Emitted when a variable is mutated, so the host can observe state changes.
// -----------------------------------------------------------------------------

// Get the name of the variable that changed.
SKALD_API const char *
skald_notification_get_var_name(const SkaldResponse *response);

// Check whether the notification carries a resolved value.
SKALD_API bool skald_notification_has_value(const SkaldResponse *response);

// Get the value type (only meaningful when skald_notification_has_value()).
SKALD_API SkaldValueType
skald_notification_get_type(const SkaldResponse *response);

// Typed accessors for the notification value.
SKALD_API const char *
skald_notification_get_string(const SkaldResponse *response);
SKALD_API bool skald_notification_get_bool(const SkaldResponse *response);
SKALD_API int skald_notification_get_int(const SkaldResponse *response);
SKALD_API float skald_notification_get_float(const SkaldResponse *response);

#ifdef __cplusplus
}
#endif

#endif // SKALD_C_H
