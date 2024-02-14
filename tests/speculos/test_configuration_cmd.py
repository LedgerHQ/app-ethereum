
def test_configuration(cmd):
    assert cmd.get_configuration() == (2, 1, 10, 4)
