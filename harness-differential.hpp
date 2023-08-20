namespace harness {
    namespace differential {
        inline void Run(const uint8_t* data, size_t size) {
            Native_Eip4788_Reset();
            const uint8_t** data_ = &data;
            Storage storage;

            while ( true ) {
                const auto input = Input::Extract(data_, size, storage);
                if ( input == std::nullopt ) return;

                ExecutionResult cpp, native;

                /* Run the C++ implementation */
                {
                    auto inp = *input;
                    const auto ret = Eip4788::run(inp, storage);
                    const auto hash = storage.Hash();
                    cpp = {.ret = ret, .hash = hash};
                }

                /* Run the canonical bytecode implementation */
                {
                    auto jsonStr = input->Json(storage).dump();
                    const auto inp = util::ToGoSlice(
                            jsonStr.data(),
                            jsonStr.size());
                    Native_Eip4788_Run(inp);
                    const auto res = util::load(Native_Eip4788_Result());
                    native = ExecutionResult::FromJson(res);
                }

                assert(cpp == native);
            }
        }
    }
}
