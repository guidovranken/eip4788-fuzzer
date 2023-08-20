class Eip4788 {
    public:
        static ReturnValue run(const Input& input, Storage& storage) {
            if ( input.caller == constants::SYSTEM_ADDRESS ) {
                return set(input, storage);
            } else {
                return get(input, storage);
            }
        }

        static ReturnValue get(const Input& input, const Storage& storage) {
            if ( input.calldata.size() != 32 ) {
                return ReturnValue::revert();
            }

            const auto calldata_u256 = util::load(input.calldata);

            const auto timestamp_idx =
                calldata_u256 %
                constants::HISTORICAL_ROOTS_MODULUS;
            const auto timestamp = storage.Get(timestamp_idx, true);

            if ( timestamp != calldata_u256 ) {
                return ReturnValue::revert();
            }

            const auto root_idx = timestamp_idx + constants::HISTORICAL_ROOTS_MODULUS;

            assert(timestamp_idx < root_idx);

            const auto root = storage.Get(root_idx, true);

            return ReturnValue::value(root);
        }

        static ReturnValue set(const Input& input, Storage& storage) {
            const auto timestamp_idx =
                uint256(input.timestamp) %
                constants::HISTORICAL_ROOTS_MODULUS;
            const auto root_idx = timestamp_idx + constants::HISTORICAL_ROOTS_MODULUS;

            assert(timestamp_idx < root_idx);

            storage.Set(timestamp_idx, input.timestamp, true);
            storage.Set(root_idx, util::load(input.calldata), true);
            return ReturnValue::value(Buffer{});
        }
};
