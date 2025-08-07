from typing import Dict, TypedDict, Optional
from ledgered.devices import DeviceType


class IconDict(TypedDict):
    Name: str
    Ticker: str
    Icon: Dict[DeviceType, str]


# pylint: disable=line-too-long
DYNAMIC_ICONS: Dict[int, IconDict] = {
    3: {
        "Name": "Ropsten",
        "Ticker": "ETH",
        "Icon": {
            DeviceType.NANOX: "",  # noqa: E501
            DeviceType.FLEX: "4000400021bd0100bb011f8b08000000000002ffed94bf4bc34014c7ff8643514441f2829b20e405a50e0eed26d242dd2df40f30505b6d3bb8ba74b260373b38386670a85b978274ea1f211d9cb228356df079975c9aa44db22bfd4ec97d8ef7e37bf78e68a57fa469894629f8a7b845032b99df2b8c2699443c00851994bb4dc06300c676a8adc797606b82afd327c66698654170d67710f331a5973956383fa073d4bb4bbc0d926fd23be291b5543a78e119b36c443c8be20904bc4439be2192c1ce821f9eb16d7ae25c0f65708a10e26bf4c5399e06bca285c233d67504c74b1fbf0044f82e553094610c0b7c835b28e4d9f8adf958919c8d6c97e385a82d0b4bbce076c833989c776e7c9551aa44de625d1ce46cceaf38d25415d1926b9e47b6cf6b826a9a7628979ab2bfa1bf4186cfd3b3f86bcc1dec495e14544593dc3f3330b0332f80a7d7bdf0add0f9c826aa08a8e13ebd89e8d1b970f9b59bde70c3f5a317e0c3dd90e518dcee1e17efd7abe710408626416b21899eaa8058e05ff598091035d6787cd3f18d5b1c00e190bac7bb6b260cb770e8847a8da4091e72870cbaeb260e70afa65bd356ca03f1704c76dafbe218a9efcb4a7f4dbf34cf947a00080000",  # noqa: E501
        },
    },
    5: {
        "Name": "Goerli",
        "Ticker": "ETH",
        "Icon": {
            DeviceType.NANOX: "",  # noqa: E501
            DeviceType.FLEX: "400040000195000093001f8b08000000000002ff85ceb10dc4300805d01fb970e9113c4a467346cb288ce092223ace80382b892ef9cd93ac0f0678881c4616d980b4bb99aa0801a5874d844ff695b5d7f6c23ad79058f79c8df7e8c5dc7d9fff13ffc61d71d7bcf32549bcef5672c5a430bb1cd6073f68c3cd3d302cd3feea88f547d6a99eb8e87ecbcdecd255b8f869033cfd932feae0c09000020000",  # noqa: E501
        },
    },
    56: {
        "Name": "BSC",
        "Ticker": "BNB",
        "Icon": {
            DeviceType.NANOX: "",  # noqa: E501
            DeviceType.FLEX: "4000400021ff0100fd011f8b08000000000002ffdd554b6ac330101d61f0225090307861c81d5c0c5d1482afe20b945c293941aed2e05d21e810012d0c42d58c64eb63d7dd475944d14833f3deccbc18f322ebf190c6e8875f32373f016a6314e32000b8e873fb0580dd8c69c1ad32336b6e0f7bba468b6501141e1e288c5be7d47e07a8aa0feba77bc70fc031b7c71e6db422f75f67d9ded20b6d8268a4649317c9fd69857022f21c85f6e790e4a3683f0a61112a214e84274278a5fd005048ccbd243e0e51720d46e08e960189d66d8470a26488c223e57e4e118eb4bff8ba4ca26a288d258121d8712311809652063617ffd0af3b47b96a5089578527b2d98c6f5d7844e70fef14e08bfacfcc44126ea48d4847070297757815e2166a75899acfb712774de2d0617dafc1cec86fbda0539e7488fa8f88f66ea8bf1c42b7dee8ac0f762c5df4fe38dba77ffcfbaff6148663c9aff4b460ee6b7c7e082c2dcd8a9f3ef0639f157bfc62d8ddfa2860a1bef5babe737fa09d6df4874748ece3466b7a25636d99fbafc0fe149ff6ac0b1a33f1d2786958684d2698e64327f39128c0325f8c8ae44ad144088d9fcf667b3edd7c9b68be674e4388541f783e08a9be3cf34152e9fd21d7a795be65eebfad3e76561fa7aeeb2ad4c7665f5fd9795f9f0bb9a9effa2f7d37231748abe08201dbf87ff0e2f843ddf7f378957f4df30b82b2cf8e00080000",  # noqa: E501
        },
    },
    137: {
        "Name": "Polygon",
        "Ticker": "POL",
        "Icon": {
            DeviceType.NANOX: "",  # noqa: E501
            DeviceType.FLEX: "400040002188010086011f8b08000000000002ffbd55416ac33010943018422f0e855c42fee01e4b09f94a7a0c3df82b3905440ffd4243a1507ae81b4c1e9047180a2604779d58f6ae7664e8250b0ec613ad7646b3aba6b949fc9a2eee8f105f7bdc14083e653d3e1f4d6f4c82f0ed809b3780e70c5f6ab8a6cf8f1b474175a41a2f8db1d7b47b7a3b2276c9506881d8cd87d7bb10afd8a2356048ec12bfe90130cc59d12d9585166f05ffdcb363295b863f21bb54662bfe85cb8ca522402bec2a92ad8d336936e9b50416e19a55e000f837d2d2a2e3f79a7d39f78aec938eb9bf549ae91378fa16a118ca982ea3febe46aa18ca90f59c14ce1c21fbd3c744e087cc582a921e7bf9552a4a761fb1461f39c5114f75f1b9732f5117499f484f813ec84386416f6f75a30b17689fd519ef3cd6f3b83114c300a7dd66b129a276d3a3a3d20209cdc09ce1fac039c50480738e09b8471dc116c18e1a18d6784eb7453bb773ee19cff9d29b37c3f744cdfc0dedc7ee0968dff7487fe97b0acf8bf3c3b48be236f772f307b129c97400080000",  # noqa: E501
        },
    },
}
# pylint: enable=line-too-long


def get_network_config(dev_type: DeviceType, chain_id: Optional[int] = None) -> tuple[str, str, bytes]:
    if chain_id and chain_id in DYNAMIC_ICONS:
        name = DYNAMIC_ICONS[chain_id]["Name"]
        ticker = DYNAMIC_ICONS[chain_id]["Ticker"]
        if dev_type == DeviceType.NANOSP:
            # Same icons
            dev_type = DeviceType.NANOX
        elif dev_type == DeviceType.STAX:
            # Same icons
            dev_type = DeviceType.FLEX
        icon = bytes.fromhex(DYNAMIC_ICONS[chain_id]["Icon"][dev_type])
    else:
        # If no chain_id is found, return empty values
        name = ""
        ticker = ""
        icon = b""

    return name, ticker, icon
