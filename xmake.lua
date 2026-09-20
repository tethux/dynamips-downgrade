set_project("dynamips")
set_version("0.2.25")
set_languages("c23", "cxx23")
set_toolchains("clang")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

local default_jit_arch = os.arch() == "x86_64" and "amd64" or "nojit"

option("dynamips_code")
    set_default("stable")
    set_values("stable", "unstable")
    set_showmenu(true)
    set_description("Select the stable or unstable emulator implementation")
option_end()

option("dynamips_arch")
    set_default(default_jit_arch)
    set_values("amd64", "x86", "ppc32", "nojit")
    set_showmenu(true)
    set_description("Select the JIT backend")
option_end()

option("enable_gen_eth")
    set_default(true)
    set_showmenu(true)
    set_description("Enable generic Ethernet through libpcap when available")
option_end()

option("enable_linux_eth")
    set_default(is_plat("linux"))
    set_showmenu(true)
    set_description("Enable Linux raw-socket Ethernet")
option_end()

option("build_nvram_export")
    set_default(true)
    set_showmenu(true)
option_end()

option("build_udp_send")
    set_default(false)
    set_showmenu(true)
option_end()

option("build_udp_recv")
    set_default(false)
    set_showmenu(true)
option_end()

add_requires("libelf")
if has_config("enable_gen_eth") then
    add_requires("libpcap", {optional = true})
end

local code = get_config("dynamips_code") or "stable"
local jit_arch = get_config("dynamips_arch") or default_jit_arch
local generated_dir = path.join("$(builddir)", "generated", code)

rule("dynamips.microcode")
    before_buildcmd_file(function (target, batchcmds, sourcefile, opt)
        local outputfile = path.join("build", "generated", code, path.basename(sourcefile) .. "_dump.inc")
        local address = path.basename(sourcefile) == "mips64_microcode" and "0xbfc00000" or "0xfff00000"
        batchcmds:show_progress(opt.progress, "${color.build.object}generating %s", outputfile)
        batchcmds:mkdir(path.directory(outputfile))
        batchcmds:vrunv(target:dep("rom2c"):targetfile(), {sourcefile, outputfile, address})
        batchcmds:add_depfiles(sourcefile, target:dep("rom2c"):targetfile())
        batchcmds:set_depmtime(os.mtime(outputfile))
        batchcmds:set_depcache(target:dependfile(outputfile))
    end)
rule_end()

local function configure_dynamips_target(target_name)
    target(target_name)
        set_warnings("all")
        set_optimize("fastest")
        add_cflags("-fasm", "-fomit-frame-pointer", {force = true})
        add_includedirs("common", code, generated_dir)
        add_includedirs("include", {public = true})
        add_defines(
            'DYNAMIPS_VERSION="0.2.25"',
            'JIT_ARCH="' .. jit_arch .. '"',
            "JIT_CPU=CPU_" .. jit_arch,
            'MIPS64_ARCH_INC_FILE="mips64_' .. jit_arch .. '_trans.h"',
            'PPC32_ARCH_INC_FILE="ppc32_' .. jit_arch .. '_trans.h"',
            "HAS_POSIX_MEMALIGN=1",
            "HAS_RFC2553=1",
            "_FILE_OFFSET_BITS=64",
            "_LARGEFILE_SOURCE",
            "_LARGEFILE64_SOURCE",
            "_DEFAULT_SOURCE",
            "OSNAME=" .. (is_plat("macosx") and "Darwin" or is_plat("windows") and "Windows" or "Linux")
        )
        add_packages("libelf", {public = true})
        if is_plat("linux") then
            add_syslinks("dl", "rt", "nsl", "pthread", {public = true})
        else
            add_syslinks("pthread", {public = true})
        end
        if code == "unstable" then
            add_defines("USE_UNSTABLE")
        end
        if has_config("enable_linux_eth") and is_plat("linux") then
            add_defines("LINUX_ETH")
        end
        if has_config("enable_gen_eth") and has_package("libpcap") then
            add_defines("GEN_ETH")
            add_packages("libpcap", {public = true})
        end
        if jit_arch == "amd64" then
            add_cflags("-m64", {force = true})
            add_ldflags("-m64", {force = true})
        elseif jit_arch == "x86" or jit_arch == "ppc32" then
            add_cflags("-m32", {force = true})
            add_ldflags("-m32", {force = true})
        end
end

target("rom2c")
    set_default(false)
    set_kind("binary")
    set_plat(os.host())
    set_arch(os.arch())
    set_policy("build.fence", true)
    add_defines("_DEFAULT_SOURCE")
    add_includedirs("common", code)
    add_packages("libelf")
    add_files("common/rom2c.c")

configure_dynamips_target("dynamips-core")
    set_kind("static")
    add_deps("rom2c")
    add_rules("dynamips.microcode")
    add_headerfiles("include/(dynamips/*.h)")
    add_files("common/*.c|bin2c.c|dynamips_main.c|nvram_export.c|ppc32_nojit_trans.c|ppc32_ppc32_trans.c|profiler.c|rom2c.c|udp_recv.c|udp_send.c")
    if not (has_config("enable_linux_eth") and is_plat("linux")) then
        remove_files("common/linux_eth.c")
    end
    if not (has_config("enable_gen_eth") and has_package("libpcap")) then
        remove_files("common/gen_eth.c")
    end
    add_files(code .. "/*.c|mips_mts.c|mips64_*_trans.c|ppc32_*_trans.c")
    add_files(code .. "/mips64_microcode", code .. "/ppc32_microcode", {rule = "dynamips.microcode"})
    if jit_arch == "ppc32" then
        add_files(code .. "/mips64_ppc32_trans.c", "common/ppc32_ppc32_trans.c")
        add_cflags("-Wa,-mregnames", {force = true})
    else
        add_files(code .. "/mips64_" .. jit_arch .. "_trans.c")
        if jit_arch == "nojit" then
            add_files("common/ppc32_nojit_trans.c")
        else
            add_files(code .. "/ppc32_" .. jit_arch .. "_trans.c")
        end
    end

configure_dynamips_target("dynamips-bindings")
    set_kind("static")
    add_deps("dynamips-core", {public = true})
    add_headerfiles("include/(dynamips/*.h)", "include/(dynamips/*.hpp)")
    add_files("bindings/*.cpp")

configure_dynamips_target("dynamips")
    set_kind("binary")
    add_deps("dynamips-core")
    add_files("common/dynamips_main.c")
    add_installfiles("ChangeLog", "COPYING", "MAINTAINERS", "README.md", "README.hypervisor", "RELEASE-NOTES", "TODO", {prefixdir = "share/doc/dynamips"})
    add_installfiles("man/dynamips.1", "man/nvram_export.1", {prefixdir = "share/man/man1"})
    add_installfiles("man/hypervisor_mode.7", {prefixdir = "share/man/man7"})

target("dynamips-hello")
    set_kind("binary")
    add_deps("dynamips-core")
    add_files("examples/hello.c")
    add_tests("hello", {trim_output = true, pass_outputs = "hello from dynamips"})

if has_config("build_nvram_export") then
    configure_dynamips_target("nvram_export")
        set_kind("binary")
        add_files("common/fs_nvram.c", "common/nvram_export.c")
end

if has_config("build_udp_send") then
    configure_dynamips_target("udp_send")
        set_kind("binary")
        add_files("common/crc.c", "common/net.c", "common/udp_send.c", "common/utils.c")
end

if has_config("build_udp_recv") then
    configure_dynamips_target("udp_recv")
        set_kind("binary")
        add_files("common/crc.c", "common/net.c", "common/udp_recv.c", "common/utils.c")
end
