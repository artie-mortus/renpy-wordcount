testsuite chat_runtime_suite:
    setup:
        $ _test.timeout = 5.0

    teardown:
        exit

    testcase channel_and_choice_runtime:
        run Jump("chat_runtime_test_target")
        advance until screen "choice"
        click "Choose A"
        advance until "Chat runtime smoke complete."
