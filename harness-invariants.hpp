namespace harness {
    namespace invariants {
        inline void Run(const uint8_t* data, size_t size) {
            Storage storage;
            const uint8_t** data_ = &data;
            std::optional<Input> prev_input;
            std::map<uint256, uint256> timestamp_calldata_map;

            while ( true ) {
                const auto input = Input::Extract(data_, size, storage, false);
                if ( input == std::nullopt ) return;

                const auto inp = *input;
                const auto prev_storage_size = storage.MapRef().size();
                const auto ret = Eip4788::run(inp, storage);
                const auto cur_storage_size = storage.MapRef().size();

                if ( input->caller == constants::SYSTEM_ADDRESS ) {
                    timestamp_calldata_map[input->timestamp] =
                        util::load(util::trim32(input->calldata));

                    /* Each call to set() should add either 0 or 2 entries
                     * to the storage
                     */
                    assert(
                            cur_storage_size == prev_storage_size ||
                            cur_storage_size == prev_storage_size + 2);

                    ::invariants::set_invariants(ret);
                } else {
                    /* Each call to get() should leave the storage unchanged */
                    assert(cur_storage_size == prev_storage_size);

                    ::invariants::get_invariants(
                            inp,
                            prev_input,
                            ret,
                            timestamp_calldata_map);
                }

                prev_input = input;
            }
        }
    }
}
