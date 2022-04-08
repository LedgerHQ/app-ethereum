
def test_configuration(cmd):
    assert cmd.get_configuration() == (14, 1, 9, 17)