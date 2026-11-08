cmake_minimum_required(VERSION 3.16)
project(SkiFreePlus C ASM)
foreach(APP SkiFreePlus SkiAuto)
 add_application(${APP} app.c sim.c gfx.c kernels.S ${CMAKE_CURRENT_BINARY_DIR}/AppResources.r TYPE APPL CREATOR SKFR)
 target_compile_options(${APP} PRIVATE -O3 -m68000 -fwrapv -fno-strict-aliasing -ffunction-sections -fdata-sections -Werror)
 set_target_properties(${APP} PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
endforeach()
target_compile_definitions(SkiAuto PRIVATE AUTOTEST=1)

foreach(BANK original)
 add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${BANK}-bank.bin
 COMMAND ${CMAKE_ASM_COMPILER} -m68000 -c ${CMAKE_CURRENT_SOURCE_DIR}/${BANK}-bank.S -o ${CMAKE_CURRENT_BINARY_DIR}/${BANK}-bank.o
 COMMAND ${CMAKE_OBJCOPY} -O binary --only-section=.text ${CMAKE_CURRENT_BINARY_DIR}/${BANK}-bank.o ${CMAKE_CURRENT_BINARY_DIR}/${BANK}-bank.bin
 DEPENDS ${BANK}-bank.S)
endforeach()
add_custom_target(sprite_banks DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/AppResources.r)
add_dependencies(SkiFreePlus sprite_banks)
add_dependencies(SkiFreePlus_APPL sprite_banks)
add_dependencies(SkiAuto sprite_banks)
add_dependencies(SkiAuto_APPL sprite_banks)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/AppResources.r
 COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/bank_resources.py ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
 DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/original-bank.bin App.r Icon.r bank_resources.py)
