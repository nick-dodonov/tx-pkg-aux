add_rules("mode.debug", "mode.release")
set_languages("c++17")

add_requires("lwlog")

target("tx-pkg-aux", function()
    set_kind("static")

    add_files("src/Log/Log.cpp")
    add_headerfiles("src/Log/Log.h", {prefixdir = "Log"})

    add_includedirs("src", {public = true})

    add_packages("lwlog")
end)
