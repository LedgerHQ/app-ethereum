llvm-profdata merge -sparse *.profraw -o default.profdata
llvm-cov show build/fuzzer_tlv -instr-profile=default.profdata -format=html > report.html
llvm-cov report build/fuzzer_tlv -instr-profile=default.profdata
