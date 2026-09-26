
target("miner-opc-tests")
do
    set_kind("binary")
    set_default(false)
    add_files("miner_opc_tests.cc")
    add_includedirs("../mods/src")
end

target("fleet-arrival-tests")
do
    set_kind("binary")
    set_default(false)
    add_files("fleet_arrival_tracker.cc")
    add_includedirs("../mods/src")
end
