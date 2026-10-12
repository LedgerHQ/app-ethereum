# Generic Clear-Signing

## Implementation

The metadata of a field (token, NFT, enum value, map entry, trusted name) is sent before the field
that uses it. An enum value or a map entry is for a function of a contract or for a primary type of
the domain of a typed message, which its tags tell: it can come before the TRANSACTION INFO of the
operation it is for.

```mermaid
sequenceDiagram
    participant HW as Hardware Wallet
    participant SW as Software Wallet

    SW ->> HW: SIGN, store only
    note over HW: store compressed<br/>calldata in RAM
    HW ->> SW: OK / KO
    SW ->> HW: TRANSACTION INFO
    HW ->> SW: OK / KO
    loop N times for N fields
        SW ->> HW: token / NFT / enum / trusted name metadatas
        HW ->> SW: OK / KO
        SW ->> HW: TX FIELD DESCRIPTION
        HW ->> SW: OK / KO
    end
    SW ->> HW: SIGN, start flow
    note left of HW: check computed fields hash
    note over HW: display all the<br/>formatted fields
    HW ->> SW: OK (with r,s,v) / KO
```

## The case of nested calldatas

```mermaid
sequenceDiagram
    participant HW as Hardware Wallet
    participant SW as Software Wallet

    SW ->> HW: SIGN, store only
    note over HW: store compressed<br/>calldata in RAM
    HW ->> SW: OK / KO
    SW ->> HW: TRANSACTION INFO (ROOT)
    HW ->> SW: OK / KO

    SW ->> HW: TX FIELD DESCRIPTION (RAW)
    HW ->> SW: OK / KO

    SW ->> HW: TX FIELD DESCRIPTION (CALLDATA)
    activate HW
    note left of HW: push into linked list of nested calldatas
    HW ->> SW: OK / KO

    SW ->> HW: NESTED - TRANSACTION INFO
    HW ->> SW: OK / KO

    SW ->> HW: NESTED - TX FIELD DESCRIPTION (AMOUNT)
    HW ->> SW: OK / KO
    deactivate HW

    SW ->> HW: TX FIELD DESCRIPTION (DATETIME)
    HW ->> SW: OK / KO

    SW ->> HW: SIGN, start flow
    note left of HW: check computed fields hash
    note over HW: display all the<br/>formatted fields
    HW ->> SW: OK (with r,s,v) / KO
```

## The case of typed messages (EIP-712 V2)

An EIP-712 V2 message is described like a transaction, from the same
[ERC-7730](https://eips.ethereum.org/EIPS/eip-7730) description. Its `EIP712_MESSAGE_INFO` takes
the place of the `TRANSACTION_INFO`, and its descriptors (`P2 = 02`) read their values in the
message with an [EIP712_PATH](tlv_structs.md#eip712_path). The calls it makes are described as
transactions (`P2 = 00`), between its descriptors.

```mermaid
sequenceDiagram
    participant HW as Hardware Wallet
    participant SW as Software Wallet

    SW ->> HW: EIP712 SEND STRUCT DEFINITION (V2)
    HW ->> SW: OK / KO
    SW ->> HW: EIP712 SEND STRUCT IMPLEMENTATION (V2)
    HW ->> SW: OK / KO
    SW ->> HW: MESSAGE INFO (TRANSACTION INFO, P2 = 02)
    HW ->> SW: OK / KO

    loop N times for N descriptors of the message
        SW ->> HW: token / NFT / enum / trusted name metadatas
        HW ->> SW: OK / KO
        SW ->> HW: FIELD DESCRIPTION (P2 = 02)
        HW ->> SW: OK / KO
        opt the descriptor is a CALLDATA, naming a call
            SW ->> HW: TRANSACTION INFO of the call (P2 = 00)
            HW ->> SW: OK / KO
            loop for each descriptor of the call
                SW ->> HW: token / NFT / enum / trusted name metadatas
                HW ->> SW: OK / KO
                SW ->> HW: FIELD DESCRIPTION (P2 = 00)
                HW ->> SW: OK / KO
            end
        end
    end

    SW ->> HW: EIP712 SIGN (V2)
    note left of HW: check the descriptors received<br/>against the announced hash,<br/>and the calls described
    note over HW: display all the<br/>formatted fields
    HW ->> SW: OK (with r,s,v) / KO
```

The metadata of a field is sent after the info of the operation the field belongs to, the message
info or the `TRANSACTION INFO` of a call, and before the descriptor that uses it.

The calls may also all be described after the last descriptor of the message, in the order they
were named. What is shown follows the order of reception.
