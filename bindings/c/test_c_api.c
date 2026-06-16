// test_c_api.c - Verify the C API works from pure C
// Build: cc -o test_c_api test_c_api.c -L../../build -lskald
// -Wl,-rpath,../../build

#include "skald_c.h"
#include <stdio.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s <skald_file>\n", argv[0]);
    return 1;
  }

  printf("Creating engine...\n");
  SkaldEngine *engine = skald_engine_new();
  if (!engine) {
    printf("Failed to create engine\n");
    return 1;
  }

  printf("Loading: %s\n", argv[1]);
  if (skald_engine_load(engine, argv[1]) != SKALD_OK) {
    printf("Failed to load module: %s\n", argv[1]);
    skald_engine_free(engine);
    return 1;
  }

  // Global state API smoke check: setting an undefined global must fail with
  // VAR_UNDEFINED, and getting an undefined global must return false.
  SkaldErrorCode set_err =
      skald_engine_set_global_int(engine, "__does_not_exist__", 1);
  SkaldValueType gt;
  bool got = skald_engine_get_global(engine, "__does_not_exist__", &gt);
  printf("Global API: set undefined -> code %d, get undefined -> %s\n", set_err,
         got ? "found" : "not found");
  if (set_err != SKALD_ERR_VAR_UNDEFINED || got) {
    printf("  WARNING: unexpected global API behavior\n");
  }

  printf("Starting...\n");
  SkaldResponse *resp = skald_engine_start(engine);

  while (resp && skald_response_type(resp) != SKALD_RESPONSE_END) {
    switch (skald_response_type(resp)) {
    case SKALD_RESPONSE_CONTENT: {
      const char *attr = skald_content_get_attribution(resp);
      const char *text = skald_content_get_text(resp);

      if (attr[0])
        printf("[%s]: ", attr);
      printf("%s\n", text);

      skald_response_free(resp);
      resp = skald_engine_act(engine, 0); // Always pick first/continue
      break;
    }
    case SKALD_RESPONSE_OPTION_GROUP: {
      size_t opt_count = skald_option_group_get_count(resp);
      printf("  Choices:\n");
      for (size_t i = 0; i < opt_count; i++) {
        const char *opt_text = skald_option_group_get_text(resp, i);
        bool avail = skald_option_group_get_available(resp, i);
        printf("    %zu. %s %s\n", i, opt_text, avail ? "" : "(unavailable)");
      }

      skald_response_free(resp);
      resp = skald_engine_act(engine, 0); // Always pick first
      break;
    }
    case SKALD_RESPONSE_METHOD_CALL_GET: {
      const char *method = skald_query_get_method(resp);
      printf("[CALL: %s (get)]\n", method);

      skald_response_free(resp);
      // A GET expects a return value; answer with null for now.
      resp = skald_engine_answer_null(engine);
      break;
    }
    case SKALD_RESPONSE_METHOD_CALL_POST: {
      const char *method = skald_query_get_method(resp);
      printf("[CALL: %s (post)]\n", method);

      skald_response_free(resp);
      // A POST is fire-and-forget; advance with act(), not answer().
      resp = skald_engine_act(engine, 0);
      break;
    }
    case SKALD_RESPONSE_NOTIFICATION: {
      printf("[NOTIFY: %s]\n", skald_notification_get_var_name(resp));
      skald_response_free(resp);
      resp = skald_engine_act(engine, 0);
      break;
    }
    case SKALD_RESPONSE_ERROR: {
      printf("ERROR %d at line %zu: %s\n", skald_error_get_code(resp),
             skald_error_get_line(resp), skald_error_get_message(resp));
      skald_response_free(resp);
      resp = NULL;
      break;
    }
    case SKALD_RESPONSE_GO_MODULE: {
      printf("[GO MODULE: %s > %s]\n", skald_go_module_get_path(resp),
             skald_go_module_get_tag(resp));
      skald_response_free(resp);
      resp = NULL; // Would need to load new module
      break;
    }
    case SKALD_RESPONSE_EXIT: {
      printf("[EXIT]\n");
      skald_response_free(resp);
      resp = NULL;
      break;
    }
    default:
      skald_response_free(resp);
      resp = NULL;
    }
  }

  if (resp)
    skald_response_free(resp);
  skald_engine_free(engine);
  printf("Done.\n");
  return 0;
}
