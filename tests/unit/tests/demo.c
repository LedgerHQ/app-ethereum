#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

int local_strchr_demo(char *string, char ch);

static void null_test_success(void **state) {
    assert_int_equal(local_strchr_demo("salut", 'c'), -1);
    assert_int_equal(local_strchr_demo("av", 'a'), 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(null_test_success),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
