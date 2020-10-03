Import("env", "projenv")

# access to global build environment
# print(env)

# access to project build environment (is used source files in "src" folder)
# print(projenv)
# my_flags = env.ParseFlags(env['BUILD_FLAGS'])
# defines = {k: v for (k, v) in my_flags.get("CPPDEFINES")}
# print(defines,sep='}{',end='\n\n')
# please keep $SOURCE variable, it will be replaced with a path to firmware

# Generic
env.Replace(
    UPLOADER="scp",
    UPLOADCMD="$UPLOADER $SOURCE pringlei:/var/www/iot-firmware/${UPLOADERFLAGS}_`git rev-parse HEAD`.bin"
)

# In-line command with arguments
# env.Replace(
#     UPLOADCMD="executable -arg1 -arg2 $SOURCE"
# )

# Python callback
# def on_upload(source, target, env):
#     print(source, target)
#     # result of print: <SCons.Node.FS.File object at 0x1306ea0>, <SCons.Node.Alias.Alias object at 0x7fb18a939640>
#     firmware_path = str(source[0])
#     print(firmware_path)
#     CPPDEFINES=env["CPPDEFINES"]
#     print(CPPDEFINES)
#     print('"$PIOENV"')
#     # ARDUINO_BOARD = CPPDEFINES['ARDUINO_BOARD']
#     # print(ARDUINO_BOARD)
#     # Send the firmware to pringlei
#     env.Execute("scp "+firmware_path+" pringlei:/media/firmware/test.bin")

# env.Replace(UPLOADCMD=on_upload)