Import("env", "projenv")

# access to global build environment
print("env: ")
print(env.Dump())

# access to project build environment (is used source files in "src" folder)
print("projenv: ")
print(projenv.Dump())
print("------------------------------------------")
print("dollaSOURCE=$SOURCE")
#
# Dump build environment (for debug purpose)
# print("env.Dump(): ",env.Dump())
#

#
# Change build flags in runtime
#
# env.ProcessUnFlags("-DVECT_TAB_ADDR")
# env.Append(CPPDEFINES=("VECT_TAB_ADDR", 0x123456789))

#
# Upload actions
#

def before_buildprog(source, target, env):
    print("before_buildprog]\nsource: ")
    print(source) 
    print(" target: ")
    print(target)
    print(" env: ")
    print(env.Dump())
    # do some actions

    # call Node.JS or other script
    # env.Execute("node --version")


def after_buildprog(source, target, env):
    print("---after_buildprog---\nsource:\n----------------- ")
    print(source) 
    print("-----------------\n target:\n----------------- ")
    print(target)
    print("-----------------\n env:\n-----------------")
    print(env.Dump())
    # do some actions

# print("Current build targets:")
# print(list(map(str, BUILD_TARGETS)))
# print("Board: ")
# print(BOARD)
# for k, v in btmap.iter():
#     print( k, v )

env.AddPreAction("buildprog", before_buildprog)
env.AddPostAction("buildprog", after_buildprog)

#
# Custom actions when building program/firmware
#

# env.AddPreAction("buildprog", callback...)
# env.AddPostAction("buildprog", callback...)

#
# Custom actions for specific files/objects
#

# env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", [callback1, callback2,...])
# env.AddPostAction("$BUILD_DIR/${PROGNAME}.hex", callback...)

# # custom action before building SPIFFS image. For example, compress HTML, etc.
# env.AddPreAction("$BUILD_DIR/spiffs.bin", callback...)

# # custom action for project's main.cpp
# env.AddPostAction("$BUILD_DIR/src/main.cpp.o", callback...)

# # Custom HEX from ELF
# env.AddPostAction(
#     "$BUILD_DIR/${PROGNAME}.elf",
#     env.VerboseAction(" ".join([
#         "$OBJCOPY", "-O", "ihex", "-R", ".eeprom",
#         "$BUILD_DIR/${PROGNAME}.elf", "$BUILD_DIR/${PROGNAME}.hex"
#     ]), "Building $BUILD_DIR/${PROGNAME}.hex")
# )