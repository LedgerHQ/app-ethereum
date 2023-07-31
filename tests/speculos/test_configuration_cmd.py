
def test_configuration(cmd):
    if cmd.model == "nanos":
        assert cmd.get_configuration() == (14, 1, 10, 3)

    if cmd.model == "nanox":
        assert cmd.get_configuration() == (14, 1, 10, 3)

    if cmd.model == "nanosp":
        assert cmd.get_configuration() == (14, 1, 10, 3)
