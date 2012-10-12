import platform

AudioLibs = []
if platform.system() == "Darwin":
    AudioLibs = ["AudioToolbox"]

Program("morse.cpp", FRAMEWORKS=AudioLibs)
Program("koch.cpp")
