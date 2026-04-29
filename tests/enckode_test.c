#define _POSIX_C_SOURCE 200809L

#include <check.h>
#include <stdio.h>
#include <string.h>

#define main main_undertest
#include "../src/hackode/enckode.c"
#undef main

START_TEST(test_copy_stream_success) {
    FILE *in = tmpfile();
    FILE *out = tmpfile();
    char buf[64] = {0};
    const char *text = "alpha\nbeta\n";

    ck_assert_ptr_nonnull(in);
    ck_assert_ptr_nonnull(out);
    ck_assert_uint_eq(fwrite(text, 1, strlen(text), in), strlen(text));
    rewind(in);

    ck_assert_int_eq(copy_stream(in, out), 0);
    rewind(out);

    ck_assert_uint_eq(fread(buf, 1, sizeof(buf) - 1, out), strlen(text));
    ck_assert_str_eq(buf, text);

    fclose(in);
    fclose(out);
}
END_TEST

START_TEST(test_copy_file_missing) {
    ck_assert_int_eq(copy_file("enckode-test", "/definitely/not/found"), -1);
}
END_TEST

START_TEST(test_split_stdin_args_basic) {
    char input[] = "one two\tthree\n";
    char **argv = NULL;
    int argc = 0;

    ck_assert_int_eq(split_stdin_args(input, &argv, &argc), 0);
    ck_assert_int_eq(argc, 3);
    ck_assert_str_eq(argv[0], "one");
    ck_assert_str_eq(argv[1], "two");
    ck_assert_str_eq(argv[2], "three");

    free(argv);
}
END_TEST

START_TEST(test_split_stdin_args_invalid) {
    char input[] = "x";
    char **argv = NULL;
    int argc = 0;

    ck_assert_int_eq(split_stdin_args(NULL, &argv, &argc), -1);
    ck_assert_int_eq(split_stdin_args(input, NULL, &argc), -1);
    ck_assert_int_eq(split_stdin_args(input, &argv, NULL), -1);
}
END_TEST

START_TEST(test_args_all_number) {
    char *ok[] = {"0", "42", "18446744073709551615"};
    char *bad[] = {"123", "x"};
    ck_assert(args_all_number(3, ok));
    ck_assert(!args_all_number(2, bad));
}
END_TEST

static Suite *enckode_suite(void) {
    Suite *s = suite_create("enckode");
    TCase *tc_core = tcase_create("core");

    tcase_add_test(tc_core, test_copy_stream_success);
    tcase_add_test(tc_core, test_copy_file_missing);
    tcase_add_test(tc_core, test_split_stdin_args_basic);
    tcase_add_test(tc_core, test_split_stdin_args_invalid);
    tcase_add_test(tc_core, test_args_all_number);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void) {
    Suite *s = enckode_suite();
    SRunner *sr = srunner_create(s);
    int failed;

    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return failed == 0 ? 0 : 1;
}
