namespace invariants {
    namespace set {
        /* set() must never revert */
        static void never_revert(
                const ReturnValue& ret) {
            assert(ret.reverted == false);
        }

        /* set() must always return empty array */
        static void always_return_empty(
                const ReturnValue& ret) {
            assert(ret.data.empty());
        }
    }

    static void set_invariants(
            const ReturnValue& ret) {
        set::never_revert(ret);
        set::always_return_empty(ret);
    }

    namespace get {
        /* get() must always revert if input is not 32 bytes */
        static void revert_if_not_32(
                const Input& input,
                const ReturnValue& ret) {
            if ( input.calldata.size() != 32 ) {
                assert(ret.reverted == true);
            }
        }

        /* get() must always return 32 bytes if it didn't revert */
        static void return_32_if_not_revert(
                const ReturnValue& ret) {
            if ( ret.reverted == false ) {
                assert(ret.data.size() == 32);
            }
        }

        static void symmetry(
                const Input& input,
                const std::optional<Input>& prev_input,
                const ReturnValue& ret) {
            /* If there was a function call before the current one */
            if ( !prev_input ) {
                return;
            }

            /* and the current call to get() was well-formed */
            if ( input.calldata.size() != 32 ) {
                return;
            }

            /* and the previous call was to set() */
            if ( prev_input->caller != constants::SYSTEM_ADDRESS ) {
                return;
            }

            /* and the timestamp during set() equals the current calldata */
            if ( uint256(prev_input->timestamp) != util::load(input.calldata) ) {
                return;
            }

            /* then get() shouldn't revert */
            assert(ret.reverted == false);

            /* and get() should return set()'s (trimmed, zero-padded) calldata */
            assert(ret.data == util::trim32(prev_input->calldata));

            /* Invariant in pseudocode:
             *
             * For any 64-bit TS:
             *
             * set(timestamp = TS);
             * assert(get(calldata = TS) == TS);
             */
        }

        static void integrity(
                const Input& input,
                const ReturnValue& ret,
                const std::map<uint256, uint256>& timestamp_calldata_map) {
            if ( ret.reverted == true ) {
                return;
            }

            const auto get_param = util::load(input.calldata);

            if ( timestamp_calldata_map.count(get_param) == 0 ) {
                return;
            }

            const auto get_res = util::load(ret.data);
            assert(timestamp_calldata_map.at(get_param) == get_res);
        }
    }

    static void get_invariants(
            const Input& input,
            const std::optional<Input>& prev_input,
            const ReturnValue& ret,
            const std::map<uint256, uint256>& timestamp_calldata_map) {
        get::revert_if_not_32(input, ret);
        get::return_32_if_not_revert(ret);
        get::symmetry(input, prev_input, ret);
        get::integrity(input, ret, timestamp_calldata_map);
    }
}
