import os
import SCons.Builder


def setupToolchain(targetEnv):
    CROSS_PREFIX = 'm68k-atari-mint-'
    targetEnv["CC"] = CROSS_PREFIX + 'gcc'
    targetEnv["AS"] = CROSS_PREFIX + 'as'
    targetEnv['CCFLAGS'] = '-m68000 -O3 -std=gnu99 -fomit-frame-pointer -ffast-math -I${TARGET.dir} '
    targetEnv['ASFLAGS'] = '-m68000'
    targetEnv['LINKFLAGS'] = '-m68000 -O3 -s '

    # Add sensible toolchain detection?
    targetEnv['ENV']['PATH'] = "/opt/cross-mint/bin:" + targetEnv['ENV']['PATH']

    return targetEnv

def detectLibCMini(targetEnv):
    libcminiPath = os.environ.get('LIBCMINI')
    if libcminiPath:
        print ("Using libcmini in: " + libcminiPath)
        targetEnv.Append(LIBS=['cmini', 'gcc'])
        targetEnv.Append(LIBPATH=[os.path.abspath(libcminiPath)])
        targetEnv.Append(CCFLAGS='-nostdlib ')
        targetEnv.Append(LINKFLAGS='-nostdlib' + ' ' + libcminiPath + '/startup.o')
        targetEnv.Append(CPPPATH=libcminiPath + '/../include')
    else:
        print("Libcmini not found, using default libs.")

def compressProgramMaybe(env, target):
    if not os.environ.get('NOUPX'):
        upx = env.WhereIs('upx')
        if upx:
            print("UPX detected, compressing target.")
            env.AddPostAction(target, Action('upx -qqq --best $TARGET'))
        else:
            print("UPX not found, skipping compression.")
    else:
        print("UPX compression is disabled")

def setFastRamFlags(env, target):
    env.AddPostAction(target, Action('m68k-atari-mint-flags --mfastram --mfastload --mfastalloc $TARGET'))

def getVersion(env):
    git = env.WhereIs('git')
    if git:
        import subprocess
        p = subprocess.Popen('git rev-list --count master', shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        return p.stdout.readline().rstrip().decode("utf-8")
    else:
        print("git not found")


# Move scons database to builddir to avoid pollution
builddir = os.path.abspath(GetLaunchDir())
SConsignFile(os.path.join(builddir, '.sconsign.dblite'))

hostEnv = Environment(ENV = {'PATH' : os.environ['PATH']})
targetEnv = setupToolchain(hostEnv.Clone())

# Optionally use libcmini
detectLibCMini(targetEnv)

targetEnv.Append(CPPDEFINES={'VERSION' : getVersion(hostEnv)})
targetEnv.Append(CPPDEFINES={'DEBUG' : 0})
targetEnv.Append(CPPDEFINES={'DUIP_CONF_BYTE_ORDER' : "BIG_ENDIAN"})

print("Building in: " + builddir)

def buildDriverVariant(hostEnv, targetEnv, varian_name):
    target = hostEnv.SConscript(
        "src/SConscript",
        duplicate = 0,
        exports=['hostEnv', 'targetEnv', 'varian_name'],
        variant_dir = builddir + '/' + varian_name,
        src_dir = "../" )
    # Optionally compress the binary with UPX
    compressProgramMaybe(targetEnv, target)
    # Load into TT ram if possible
    setFastRamFlags(targetEnv, target)
    return target

# Netusbee binary
targetNetusbee = buildDriverVariant(hostEnv, targetEnv, "netusbee")

# USB ASIX binary
usbTartegEnv = targetEnv.Clone()
usbTartegEnv.Append(CPPDEFINES={'USB_DRIVER':1})
usbTartegEnv.Append(CPPDEFINES={'USB_PRINTSTATUS':1})
targetUSB = buildDriverVariant(hostEnv, usbTartegEnv, "usb")

num_cpu = int(os.environ.get('NUMBER_OF_PROCESSORS', 2))
SetOption('num_jobs', num_cpu)
print("running with %d jobs." % GetOption('num_jobs')) 

targets = [targetNetusbee, targetUSB]

Default(targets)
