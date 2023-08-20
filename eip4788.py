import json

HISTORICAL_ROOTS_MODULUS = 98304
SYSTEM_ADDRESS = 0xfffffffffffffffffffffffffffffffffffffffe

class Uint256(object):
    def __init__(self, v):
        if isinstance(other, self.__class__):
            v = other.v
        elif isinstance(other, str):
            v = int(v, 16)
        elif isinstance(other, bytes):
            assert(len(other) == 32)
            # TODO
        self.v = v % (2**256)
    def __eq__(self, other):
        if isinstance(other, self.__class__):
            return self.v == other.v
        elif isinstance(other, int):
            return self.v == v
        else:
            assert(False)
    def __mod__(self, other):
        return Uint256(self.v % other.v)
    def __add__(self, other):
        return Uint256(self.v + other.v)

def to_uint256_be(v):
    return Uint256(v)

# Mock EVM storage
class Storage(object):
    def __init__(self):
        self.map = {}
    def get(self, address):
        if address in self.map:
            return self.map[address]
        else:
            return 0
    def set(self, address, value):
        self.map[address] = value
    def hash(self):
        pass

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

    def __init__(self, caller, calldata, timestamp):
        self.reverted = False
        self.caller = caller
        self.calldata = calldata
        self.timestamp = timestamp
        self.returndata = bytes()
    def revert(self):
        self.reverted = True
        raise Revert()
    def return_(self, v):
        self.returndata = v
        raise Return()
    def ExecutionResult(self):
        j = {}
        j['ret'] = {}
        j['ret']['reverted'] = self.reverted
        j['ret']['data'] = self.data
        j['hash'] = 0
        return j.dumps()

def Eip4788(evm):
    # Verbatim copy from eip-4788.md except:
    #
    # - "to_uint256_be(timestamp)" in get() changed to "to_uint256_be(evm.calldata)"
    # - "evm.return" changed to "evm.return_"
    def get():
        if len(evm.calldata) != 32:
            evm.revert()

        timestamp_idx = to_uint256_be(timestamp) % HISTORICAL_ROOTS_MODULUS
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
            j['timestamp'])

    try:
        Eip4788(evm)
    except EVM.EVMException:
        pass
