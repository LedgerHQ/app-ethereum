# Documentation of Ethereum's client test

```sh
.
├── conftest.py                                     # Configuration for pytest
├── ethereum_client                                 # All utils of client test
│   ├── ethereum_cmd_builder.py                     # Creation of apdu to send
│   ├── ethereum_cmd.py                             # Send Apdu and parsing of response
│   ├── exception
│   │   ├── device_exception.py
│   │   └── errors.py
│   ├── plugin.py                                   # Creation of content apdu which manage plugin, erc20Information, provide nft information
│   ├── transaction.py                              # Creation of content apdu which manage personal tx, transaction, eip712
│   └── utils.py
├── requirements.txt
├── screenshots                                     # All screenshot of nanoS,X,SP for compare in tests
├── setup.cfg

# ========= All Tests =========
├── test_configuration_cmd.py
├── test_eip1559.py
├── test_eip191.py
├── test_eip2930.py
├── test_eip712.py
├── test_erc1155.py
├── test_erc20information.py
├── test_erc721.py
├── test_pubkey_cmd.py
└── test_sign_cmd.py
```

## Ethereum_client

### Ethereum_cmd_builder
```py
def chunked(size, source)

class EthereumCommandBuilder:
    # Creation of the apdu
    def get_configuration(self) -> bytes:
    def set_plugin(self, plugin: Plugin) -> bytes:
    def provide_nft_information(self, plugin: Plugin) -> bytes:
    def provide_erc20_token_information(self, info: ERC20Information):
    def get_public_key(self, bip32_path: str, display: bool = False) -> bytes:
    def perform_privacy_operation(self, bip32_path: str, display: bool, shared_secret: bool) -> bytes:
    def simple_sign_tx(self, bip32_path: str, transaction: Transaction) -> bytes:
    def sign_eip712(self, bip32_path: str, transaction: EIP712) -> bytes:
    def personal_sign_tx(self, bip32_path: str, transaction: PersonalTransaction) -> Tuple[bool,bytes]:
```

### Ethereum_cmd
```py
class EthereumCommand:
    # Sending apdu and parsing the response in the right form
    def get_configuration(self) -> Tuple[int, int, int, int]:
    def set_plugin(self, plugin: Plugin):
    def provide_nft_information(self, plugin: Plugin):
    def provide_erc20_token_information(self, info: ERC20Information):
    def get_public_key(self, bip32_path: str, result: List, display: bool = False) -> Tuple[bytes, bytes, bytes]:
    def perform_privacy_operation(self, bip32_path: str, result: List, display: bool = False, shared_secret: bool = False) -> Tuple[bytes, bytes, bytes]:
    def simple_sign_tx(self, bip32_path: str, transaction: Transaction, result: List = list()) -> None:
    def sign_eip712(self, bip32_path: str, transaction: EIP712, result: List = list()) -> None:
    def personal_sign_tx(self, bip32_path: str, transaction: PersonalTransaction, result: List = list()) -> None:
    

    # Allows to send an apdu without return of speculos
    def send_apdu(self, apdu: bytes) -> bytes:
    # Allows to send an apdu with return of speculos
    def send_apdu_context(self, apdu: bytes, result: List = list()) -> bytes:
    
```

### Utils
```py
def save_screenshot(cmd, path: str):
def compare_screenshot(cmd, path: str):
def parse_sign_response(response : bytes) -> Tuple[bytes, bytes, bytes]:
def bip32_path_from_string(path: str) -> List[bytes]:
def packed_bip32_path_from_string(path: str) -> bytes:
def write_varint(n: int) -> bytes:
def read_varint(buf: BytesIO, prefix: Optional[bytes] = None) -> int:
def read(buf: BytesIO, size: int) -> bytes:
def read_uint(buf: BytesIO,
```

## Tests new apdu

If a new instruction is programmed it will be necessary to create 2 new functions.  
one in `ethereum_cmd_builder` :  
- Creation of the raw apdu you can find some examples in this same file
  
and one in `ethereum_cmd`:  
- Send the apdu to speculos and parse the answer in a `list` named result you can find some examples in this same file

## Example for write new tests

To send several apdu and get the return 

```py
FIRST = bytes.fromhex("{YourAPDU}")
SECOND = bytes.fromhex("{YourAPDU}")

def test_multiple_raw_apdu(cmd):
    result: list = []

    cmd.send_apdu(FIRST)
    with cmd.send_apdu_context(SECOND, result) as ex:
        sleep(0.5)
        # Here your code for press button and compare screen if you want

    response: bytes = result[0] # response returning
    # Here you function to parse response of some code
    v, r, s = parse_sign_response(response)

    # And here assertion of your tests
    assert v == 0x25 # 37
    assert r.hex() == "68ba082523584adbfc31d36d68b51d6f209ce0838215026bf1802a8f17dcdff4"
    assert s.hex() == "7c92908fa05c8bc86507a3d6a1c8b3c2722ee01c836d89a61df60c1ab0b43fff"
```

To test an error

```py
def test_some_error(cmd):
    result: list = []

    with pytest.raises(ethereum_client.exception.errors.UnknownDeviceError) as error:
        # With an function in ethereum_cmd
        with cmd.send_apdu_context(bytes.fromhex("{YourAPDU}"), result) as ex:
            pass
        assert error.args[0] == '0x6a80'
```

