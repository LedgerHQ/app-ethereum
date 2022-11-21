import pytest
from ethereum_client.plugin import ERC20Information
import ethereum_client

def test_provide_erc20_token(cmd):
    erc20_info = ERC20Information(
        erc20_ticker="5a5258",
        addr="0xe41d2489571d322189246dafa5ebde1f4699f498",
        nb_decimals=18,
        chainID=1,
        sign="304402200ae8634c22762a8ba41d2acb1e068dcce947337c6dd984f13b820d396176952302203306a49d8a6c35b11a61088e1570b3928ca3a0db6bd36f577b5ef87628561ff7"
    )

    # Test if return 9000
    try:
        cmd.provide_erc20_token_information(info=erc20_info)
    except:
        raise

def test_provide_erc20_token_error(cmd):
    erc20_info = ERC20Information(
        erc20_ticker="5a5258",
        addr="0xe41d2489571d322189246dafa5ebde1f4699f498",
        nb_decimals=18,
        chainID=1,
        sign="deadbeef"
    )

    with pytest.raises(ethereum_client.exception.errors.UnknownDeviceError) as error:
        cmd.provide_erc20_token_information(info=erc20_info)
        assert error.args[0] == '0x6a80'