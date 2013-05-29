#include <stdlib.h>
#include "test_suite.h"

int main(void) {
  SRunner *runner = srunner_create(make_sample_suite());
  srunner_run_all(runner, CK_NORMAL);
  int number_failed = srunner_ntests_failed(runner);
  srunner_free(runner);
  
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
