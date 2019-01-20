import os

def setupToolchain(targetEnv):   
    CROSS_PREFIX = 'm68k-atari-mint-'
    targetEnv["CC"] = CROSS_PREFIX + 'gcc'
    targetEnv['CCFLAGS'] = '-m68000 -Os -std=gnu99 -fomit-frame-pointer -ffast-math -I${TARGET.dir} '
    targetEnv['LINKFLAGS'] = '-m68000 -Os -std=gnu99 -fno-exceptions -fno-rtti -fomit-frame-pointer -s '

    # Add sensible toolchain detection?
    targetEnv['ENV']['PATH'] = "/opt/cross-mint/bin:" + targetEnv['ENV']['PATH']

    return targetEnv

def detectLibCMini(targetEnv):
    libcminiPath = os.environ.get('LIBCMINI')
    if libcminiPath:
        print "Using libcmini in: " + libcminiPath
        targetEnv.Append(LIBS=['iiomini', 'cmini', 'gcc'])
        targetEnv.Append(LIBPATH=[libcminiPath])
        targetEnv.Append(CCFLAGS='-nostdlib ')
        targetEnv.Append(LINKFLAGS='-nostdlib' + ' ' + libcminiPath + '/startup.o')
    else:
        print "Libcmini not found, using default libs."

def compressProgramMaybe(env, target):
    if not os.environ.get('NOUPX'):
        upx = env.WhereIs('upx')
        if upx:
            print "UPX detected, compressing target."
            env.AddPostAction(target, Action('upx -qqq --best $TARGET'))
        else:
            print "UPX not found, skipping compression."
    else:
        print "UPX compression is disabled"

def getVersion(env):
    git = env.WhereIs('git')
    if git:
        import subprocess
        p = subprocess.Popen('git rev-list --count master', shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        return p.stdout.readline().rstrip()
    else:
        print "git not found"


# Move scons database to builddir to avoid pollution
builddir = os.path.abspath(GetLaunchDir())
SConsignFile(os.path.join(builddir, '.sconsign.dblite'))

hostEnv = Environment(ENV = {'PATH' : os.environ['PATH']} )
targetEnv = setupToolchain(hostEnv.Clone())

# Optionally use libxmini
detectLibCMini(targetEnv)

#version:=$(shell git rev-list --count master)
targetEnv.Append(CPPDEFINES={'VERSION' : getVersion(hostEnv)})
targetEnv.Append(CPPDEFINES={'DUIP_CONF_BYTE_ORDER' : "BIG_ENDIAN"})

print "Building in: " + GetLaunchDir()

target = hostEnv.SConscript(
    "src/SConscript",
    duplicate = 0,
    exports=['hostEnv', 'targetEnv'],
    variant_dir = builddir,
    src_dir = "../" )

# Optionally compress the binary with UPX
compressProgramMaybe(targetEnv, target)

num_cpu = int(os.environ.get('NUMBER_OF_PROCESSORS', 2))
SetOption('num_jobs', num_cpu)
print "running with ", GetOption('num_jobs'), "jobs." 

Default(target)
