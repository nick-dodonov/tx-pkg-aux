add_rules("mode.debug", "mode.release")
set_languages("c++17")

target("tx-pkg-misc", function()
    set_kind("static")

    add_includedirs("src", {public = true})
end)
