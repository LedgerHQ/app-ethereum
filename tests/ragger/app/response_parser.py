class   EthereumRespParser:
    def sign(self, data: bytes):
        assert len(data) == (1 + 32 + 32)

        v = data[0:1]
        data = data[1:]

        r = data[0:32]
        data = data[32:]

        s = data[0:32]
        data = data[32:]

        return v, r, s
