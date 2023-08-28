import json

HISTORICAL_ROOTS_MODULUS = 98304
SYSTEM_ADDRESS = 0xfffffffffffffffffffffffffffffffffffffffe

class Uint256(object):
    def __init__(self, v):
        if isinstance(v, list):
            v = bytes(v)

        if isinstance(v, self.__class__):
            v = v.v
        elif isinstance(v, int):
            pass
        elif isinstance(v, str):
            v = int(v, 16)
        elif isinstance(v, bytes):
            v = int.from_bytes(v, byteorder='big')
        else:
            assert(False)
        self.v = v % (2**256)
    def to_bytes(self):
        return self.v.to_bytes(32, byteorder='big')
    def __eq__(self, other):
        if isinstance(other, self.__class__):
            return self.v == other.v
        elif isinstance(other, int):
            return self.v == other
        elif isinstance(other, bytes):
            assert(len(other) == 32)
            return self.v == to_uint256_be(other).v
        else:
            assert(False)
    def __mod__(self, other):
        if isinstance(other, int):
            other = Uint256(other)
        return Uint256(self.v % other.v)
    def __add__(self, other):
        if isinstance(other, int):
            other = Uint256(other)
        return Uint256(self.v + other.v)

def to_uint256_be(v):
    return Uint256(v)

# Mock EVM storage
class Storage(object):
    def __init__(self, kv):
        self.map = {}
        if kv == None:
            return
        for k, v in kv.items():
            self.set(Uint256(k), Uint256(v))

    def get(self, address):
        if isinstance(address, Uint256):
            address = address.v

        if address in self.map:
            return Uint256(self.map[address])
        else:
            return Uint256(0)
    def set(self, address, value):
        if isinstance(address, Uint256):
            address = address.v

        if isinstance(value, Uint256):
            value = value.v
        if isinstance(value, bytes):
            value = value[:32]
            value += b'\x00' * (32 - len(value))
            value = to_uint256_be(value)
            value = value.v
        self.map[address] = value
    def get_map(self):
        ret = {}
        for k, v in self.map.items():
            ret[str(k)] = str(v)
        return ret

# Mock EVM
class EVM(object):
    # These exceptions serve to break control flow
    # once Eip4788 calls evm.revert() or evm.return_()
    class Exception(Exception):
        pass
    class Revert(Exception):
        pass
    class Return(Exception):
        pass

    def __init__(self, caller, calldata, timestamp, storage):
        self.reverted = False
        self.caller = caller
        self.calldata = calldata
        self.timestamp = timestamp
        self.storage = Storage(storage)
        self.returndata = bytes()
    def revert(self):
        self.reverted = True
        raise EVM.Revert()
    def return_(self, v):
        if isinstance(v, Uint256):
            v = v.to_bytes()
        self.returndata = v
        raise EVM.Return()
    def ExecutionResult(self):
        j = {}
        j['Ret'] = {}
        j['Ret']['Reverted'] = self.reverted
        j['Ret']['Data'] = self.returndata.hex()
        j['Hash'] = 0
        j['Storage'] = self.storage.get_map()
        return json.dumps(j).encode('utf-8')

def Eip4788(evm):
    storage = evm.storage

    # Verbatim copy from eip-4788.md except:
    #
    # - "evm.return" changed to "evm.return_"
    def get():
        if len(evm.calldata) != 32:
            evm.revert()

        timestamp_idx = to_uint256_be(evm.calldata) % HISTORICAL_ROOTS_MODULUS
        timestamp = storage.get(timestamp_idx)

        if timestamp != evm.calldata:
            evm.revert()

        root_idx = timestamp_idx + HISTORICAL_ROOTS_MODULUS
        root = storage.get(root_idx)

        evm.return_(root)

    def set():
        timestamp_idx = to_uint256_be(evm.timestamp) % HISTORICAL_ROOTS_MODULUS
        root_idx = timestamp_idx + HISTORICAL_ROOTS_MODULUS

        storage.set(timestamp_idx, evm.timestamp)
        storage.set(root_idx, evm.calldata)

    if evm.caller == SYSTEM_ADDRESS:
        set()
    else:
        get()

def FuzzerRunOne(Input):
    j = json.loads(Input)

    evm = EVM(
            to_uint256_be(j['caller']),
            bytes(j['calldata']),
            j['timestamp'],
            j['storage'])

    try:
        Eip4788(evm)
    except EVM.Exception:
        pass

    return evm.ExecutionResult()
