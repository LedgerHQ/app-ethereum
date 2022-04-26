
def test_configuration(cmd):
    if cmd.model == "nanos":
        assert cmd.get_configuration() == (14, 1, 9, 17)
    
    if cmd.model == "nanox":
        assert cmd.get_configuration() == (14, 1, 9, 17)