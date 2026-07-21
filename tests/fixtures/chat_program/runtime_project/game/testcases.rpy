testsuite global:
    teardown:
        exit

testcase chat_runtime_suite:
    $ _test.timeout = 5.0
    run Jump("chat_runtime_test_target")
    advance until screen "choice"
    click "Choose A"
    advance until "Chat runtime smoke complete."
