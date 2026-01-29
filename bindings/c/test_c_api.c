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
  skald_engine_load(engine, argv[1]);

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

      size_t opt_count = skald_content_get_option_count(resp);
      if (opt_count > 0) {
        printf("  Choices:\n");
        for (size_t i = 0; i < opt_count; i++) {
          const char *opt_text = skald_content_get_option_text(resp, i);
          bool avail = skald_content_get_option_available(resp, i);
          printf("    %zu. %s %s\n", i, opt_text, avail ? "" : "(unavailable)");
        }
      }

      skald_response_free(resp);
      resp = skald_engine_act(engine, 0); // Always pick first/continue
      break;
    }
    case SKALD_RESPONSE_QUERY: {
      const char *method = skald_query_get_method(resp);
      printf("[QUERY: %s]\n", method);

      skald_response_free(resp);
      // Answer with null for now
      resp = skald_engine_answer_null(engine);
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
