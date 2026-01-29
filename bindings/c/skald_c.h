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

// Response type enum - matches order of std::variant<Content, Query, Exit,
// GoModule, End, Error>
typedef enum SkaldResponseType {
  SKALD_RESPONSE_CONTENT = 0,
  SKALD_RESPONSE_QUERY = 1,
  SKALD_RESPONSE_EXIT = 2,
  SKALD_RESPONSE_GO_MODULE = 3,
  SKALD_RESPONSE_END = 4,
  SKALD_RESPONSE_ERROR = 5
} SkaldResponseType;

// Error codes - match the constants in skald.hpp
typedef enum SkaldErrorCode {
  SKALD_ERR_UNKNOWN = 0,
  SKALD_ERR_EOF = 1,
  SKALD_ERR_EMPTY_MODULE = 2,
  SKALD_ERR_MODULE_TAG_NOT_FOUND = 3,
  SKALD_ERR_CHOICE_OUT_OF_BOUNDS = 4,
  SKALD_ERR_CHOICE_UNAVAILABLE = 5,
  SKALD_ERR_EXPECTED_ANSWER = 6,
  SKALD_ERR_RESOLUTION_QUEUE_EMPTY = 7,
  SKALD_ERR_TYPE_MISMATCH = 8
} SkaldErrorCode;

// =============================================================================
// Engine Lifecycle
// =============================================================================

// Create a new engine instance. Returns NULL on allocation failure.
SKALD_API SkaldEngine *skald_engine_new(void);

// Destroy an engine and free all associated memory.
SKALD_API void skald_engine_free(SkaldEngine *engine);

// Load a module from a file path.
SKALD_API void skald_engine_load(SkaldEngine *engine, const char *path);

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

// Get the number of choices available.
SKALD_API size_t skald_content_get_option_count(const SkaldResponse *response);

// Get the text of a specific option.
SKALD_API const char *
skald_content_get_option_text(const SkaldResponse *response, size_t index);

// Check if a specific option is currently available (not grayed out).
SKALD_API bool skald_content_get_option_available(const SkaldResponse *response,
                                                  size_t index);

// -----------------------------------------------------------------------------
// Query Response Accessors (only valid when type == SKALD_RESPONSE_QUERY)
// -----------------------------------------------------------------------------

// Get the method name being called.
SKALD_API const char *skald_query_get_method(const SkaldResponse *response);

// Get the number of arguments.
SKALD_API size_t skald_query_get_arg_count(const SkaldResponse *response);

// Get an argument as a string (all args are converted to string
// representation).
SKALD_API const char *skald_query_get_arg(const SkaldResponse *response,
                                          size_t index);

// Check if this query expects a return value.
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

#ifdef __cplusplus
}
#endif

#endif // SKALD_C_H
