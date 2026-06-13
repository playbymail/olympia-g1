cmake_minimum_required(VERSION 4.1)
project(olympia-g1 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# =============================================================================
# Compiler-flag conventions (read before "tidying up" this file)
# =============================================================================
# The 32->64-bit modernization is complete; every warning class below was
# triaged to ZERO and locked as an error. See CLAUDE.md "Modernization status"
# for the per-phase history.
#
# 1. Enforced classes are written as an explicit pair, one class per line:
#        -Wfoo -Werror=foo
#    The bare -Wfoo is technically redundant (-Werror=foo already enables it),
#    but the pair is deliberate: it records that the class was driven to zero
#    and locked. Do NOT collapse these to bare -Werror=foo -- the redundancy is
#    a record of completed work, not clutter.
#
# 2. -Wno-foo suppressions are legacy idioms this ~60K-line codebase still
#    relies on and that are NOT being chased down right now.
#
# Both sets, plus optional sanitizers, are applied identically to every target
# via olympia_compile_flags() / olympia_enable_sanitizers(). There is one flag
# set for the whole project -- keep it that way.
# =============================================================================

option(OLYMPIA_SANITIZE "Enable sanitizer instrumentation" OFF)
set(OLYMPIA_SANITIZERS "address,undefined" CACHE STRING
    "Comma-separated sanitizers, e.g. address,undefined")

# All engine warning flags, applied identically to every target.
function(olympia_compile_flags tgt)
    target_compile_options(${tgt} PRIVATE
        # --- Suppressions: legacy idioms left as-is ---
        -Wno-comment
        -Wno-compare-distinct-pointer-types
        -Wno-dangling-else
        -Wno-deprecated-declarations
        -Wno-extra-tokens
        -Wno-incompatible-library-redeclaration
        -Wno-logical-not-parentheses
        -Wno-macro-redefined
        -Wno-multichar
        -Wno-non-literal-null-conversion
        -Wno-parentheses
        -Wno-tautological-constant-out-of-range-compare

        # --- Format/vararg checking enforced (issue #7) ---
        # The non-literal subtypes stay off pending a dedicated hardening pass.
        -Wformat -Werror=format
        -Wno-format-security
        -Wno-format-nonliteral

        # --- Modernization classes triaged to 0 and locked (CLAUDE.md) ---
        -Wint-to-pointer-cast -Werror=int-to-pointer-cast                # Phase 1
        -Wpointer-to-int-cast -Werror=pointer-to-int-cast                # Phase 1
        -Wincompatible-pointer-types -Werror=incompatible-pointer-types  # Phase 2
        -Wint-conversion -Werror=int-conversion                          # Phase 3
        -Wstrict-prototypes -Werror=strict-prototypes                    # Phase 4
        -Wmissing-prototypes -Werror=missing-prototypes                  # Phase 4
        -Wimplicit-function-declaration -Werror=implicit-function-declaration  # Phase 4
        -Wmissing-declarations -Werror=missing-declarations              # Phase 5
        # A sizeof(ptr) where sizeof(*ptr) is meant grows 4->8 bytes on LP64
        # (a real corruption risk). Audited clean.
        -Wsizeof-pointer-memaccess -Werror=sizeof-pointer-memaccess      # Phase 6 (#10)
        # Signed<->unsigned conversions that diverge on LP64.
        -Wsign-conversion -Werror=sign-conversion                        # Phase 7 (#11)
        # A function that falls off the end leaves the caller reading a garbage
        # register -- on LP64 an 8-byte garbage long/pointer.
        -Wreturn-type -Werror=return-type                                # Phase 8 (#13)
    )

    # Clang-only diagnostic spellings; GCC folds each into a broader class
    # (-Wreturn-type / -Wconversion) covered above.
    if (CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(${tgt} PRIVATE
            # The LP64 width-truncation class (long/size_t/time_t into int).
            -Wshorten-64-to-32 -Werror=shorten-64-to-32                  # Phase 6 (#10)
            # A bare `return;` in a non-void fn (GCC folds into -Wreturn-type).
            -Wreturn-mismatch -Werror=return-mismatch                    # Phase 8 (#13)
            # Code-quality, NOT a 64-bit hazard: int->short/char narrowing into
            # entity struct fields. Identical on ILP32/LP64.
            -Wimplicit-int-conversion -Werror=implicit-int-conversion    # issue #14
        )
    endif()
endfunction()

function(olympia_enable_sanitizers tgt)
    if(NOT OLYMPIA_SANITIZE)
        return()
    endif()
    if(NOT OLYMPIA_SANITIZERS)
        message(FATAL_ERROR "OLYMPIA_SANITIZE is ON but OLYMPIA_SANITIZERS is empty")
    endif()

    # Sanitizers are supported on Clang/GCC; MSVC uses different flags.
    if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        # Compile and Link flags must match!
        target_compile_options(${tgt} PRIVATE
            -fsanitize=${OLYMPIA_SANITIZERS}
            -fno-omit-frame-pointer
            -fno-optimize-sibling-calls
            -fno-sanitize-recover=all
            -fno-common
        )
        target_link_options(${tgt} PRIVATE
            -fsanitize=${OLYMPIA_SANITIZERS}
            -fno-omit-frame-pointer
            -fno-optimize-sibling-calls
            -fno-sanitize-recover=all
            -fno-common
        )
        if (CMAKE_C_COMPILER_ID MATCHES "Clang")
            # -fsanitize-address-use-after-scope is Clang-only
            target_compile_options(${tgt} PRIVATE
                -fsanitize-address-use-after-scope
            )
            target_link_options(${tgt} PRIVATE
                -fsanitize-address-use-after-scope
            )
        endif()
    else()
        message(WARNING "OLYMPIA_SANITIZE is ON but compiler is not Clang/GNU; sanitizers not enabled.")
    endif()
endfunction()

# Platform-specific defines
if (LINUX)
    add_compile_definitions(OLYMPIA_CC_LINUX)
endif ()
if (WIN32)
    add_compile_definitions(OLYMPIA_CC_WINDOWS)
endif ()

# ------------------------------------------------------------------------------
# G1 mapgen
# ------------------------------------------------------------------------------
add_executable(mapgen-g1)
target_sources(mapgen-g1 PRIVATE
    mapgen/mapgen.c mapgen/z.c
    lib/cstrings.c lib/exit_views.c
    lib/ilist.c lib/roads.c lib/tiles.c
)
target_sources(mapgen-g1 PRIVATE
    FILE_SET HEADERS
    BASE_DIRS
        mapgen lib
    FILES
        mapgen/z.h
        lib/lists.h
)
target_include_directories(mapgen-g1 PRIVATE mapgen/ lib/)
# Require C11 at the target level (project-level CMAKE_C_STANDARD remains the
# default for future targets). See issue #6.
target_compile_features(mapgen-g1 PRIVATE c_std_11)
olympia_compile_flags(mapgen-g1)
olympia_enable_sanitizers(mapgen-g1)

# ------------------------------------------------------------------------------
# G1 olympia (main game engine)
# ------------------------------------------------------------------------------
add_executable(olympia-g1)
target_sources(olympia-g1 PRIVATE
    olympia/add.c olympia/adv.c olympia/alchem.c olympia/art.c olympia/basic.c
    olympia/beast.c olympia/build.c olympia/buy.c olympia/check.c olympia/c1.c
    olympia/c2.c olympia/cloud.c olympia/code.c olympia/combat.c olympia/day.c
    olympia/dir.c olympia/display.c olympia/eat.c olympia/faery.c olympia/garr.c
    olympia/gate.c olympia/glob.c olympia/gm.c olympia/hades.c olympia/immed.c
    olympia/input.c olympia/io.c olympia/loc.c olympia/lore.c olympia/main.c
    olympia/make.c olympia/move.c olympia/necro.c olympia/npc.c olympia/order.c
    olympia/perm.c olympia/produce.c olympia/quest.c olympia/relig.c
    olympia/report.c olympia/savage.c olympia/scry.c olympia/seed.c
    olympia/sout.c olympia/stack.c olympia/stealth.c olympia/storm.c
    olympia/summary.c olympia/swear.c olympia/u.c olympia/use.c olympia/z.c

    lib/admits.c lib/cstrings.c lib/exit_views.c lib/fights.c lib/flag_ents.c
    lib/ilist.c lib/item_ents.c
    lib/orders.c lib/req_ents.c lib/roads.c lib/skill_ents.c lib/tiles.c lib/trades.c lib/wait_args.c
)
target_sources(olympia-g1 PRIVATE
    FILE_SET HEADERS
    BASE_DIRS
        olympia lib
    FILES
        olympia/code.h olympia/dir.h olympia/display.h olympia/etc.h olympia/garr.h
	    olympia/loc.h olympia/loop.h olympia/oly.h olympia/order.h olympia/sout.h
	    olympia/stack.h olympia/swear.h olympia/u.h olympia/use.h olympia/z.h
        lib/lists.h
)
target_include_directories(olympia-g1 PRIVATE olympia/ lib/)
# Require C11 at the target level (project-level CMAKE_C_STANDARD remains the
# default for future targets). See issue #6.
target_compile_features(olympia-g1 PRIVATE c_std_11)
olympia_compile_flags(olympia-g1)
olympia_enable_sanitizers(olympia-g1)
