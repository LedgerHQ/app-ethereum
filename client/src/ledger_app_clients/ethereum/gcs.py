from typing import Optional

from .tlv import format_tlv
from .keychain import sign_data, Key


class TxInfo():
    version: int
    chain_id: int
    contract_addr: bytes
    selector: bytes
    fields_hash: bytes
    operation_type: str
    creator_name: Optional[str]
    creator_legal_name: Optional[str]
    creator_url: Optional[str]
    contract_name: Optional[str]
    deploy_date: Optional[int]
    signature: Optional[bytes]

    def __init__(self,
                 version: int,
                 chain_id: int,
                 contract_addr: bytes,
                 selector: bytes,
                 fields_hash: bytes,
                 operation_type: str,
                 creator_name: Optional[str] = None,
                 creator_legal_name: Optional[str] = None,
                 creator_url: Optional[str] = None,
                 contract_name: Optional[str] = None,
                 deploy_date: Optional[int] = None,
                 signature: Optional[bytes] = None):
        self.version = version
        self.chain_id = chain_id
        self.contract_addr = contract_addr
        self.selector = selector
        self.fields_hash = fields_hash
        self.operation_type = operation_type
        self.creator_name = creator_name
        self.creator_legal_name = creator_legal_name
        self.creator_url = creator_url
        self.contract_name = contract_name
        self.deploy_date = deploy_date
        self.signature = signature

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += format_tlv(0x00, self.version)
        payload += format_tlv(0x01, self.chain_id)
        payload += format_tlv(0x02, self.contract_addr)
        payload += format_tlv(0x03, self.selector)
        payload += format_tlv(0x04, self.fields_hash)
        payload += format_tlv(0x05, self.operation_type)
        if self.creator_name is not None:
            payload += format_tlv(0x06, self.creator_name)
        if self.creator_legal_name is not None:
            payload += format_tlv(0x07, self.creator_legal_name)
        if self.creator_url is not None:
            payload += format_tlv(0x08, self.creator_url)
        if self.contract_name is not None:
            payload += format_tlv(0x09, self.contract_name)
        if self.deploy_date is not None:
            payload += format_tlv(0x0a, self.deploy_date)
        signature = self.signature
        if signature is None:
            signature = sign_data(Key.CAL, payload)
        payload += format_tlv(0xff, signature)
        return payload
