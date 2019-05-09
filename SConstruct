import os
import SCons.Builder


def setupToolchain(targetEnv, debugEnabled):   
    CROSS_PREFIX = 'm68k-atari-mint-'
    
    CCFLAGS=''
    LDFLAGS=''
    VASMFLAGS=''

    targetEnv["CC"] = CROSS_PREFIX + 'gcc'

    if(debugEnabled):
    	print "Debug build enabled with symbols generation and optimisations turned off."
    	os.environ['NOUPX']='1'
    	CCFLAGS='-m68000 -O1 -std=gnu99 -fomit-frame-pointer -ffast-math -I${TARGET.dir} '
    	LDFLAGS='-m68000 -O1 -Wl,--traditional-format '
    	VASMFLAGS='-Faout -quiet -showopt -DDEBUG'
    else:
    	CCFLAGS='-m68000 -O3 -std=gnu99 -fomit-frame-pointer -ffast-math -I${TARGET.dir} '
    	LDFLAGS='-m68000 -O3 -s '
    	VASMFLAGS='-Faout -showopt -quiet '
    
    targetEnv['CCFLAGS'] = CCFLAGS
    targetEnv['LINKFLAGS'] = LDFLAGS

    # Add sensible toolchain detection?
    targetEnv['ENV']['PATH'] = "/opt/cross-mint/bin:" + targetEnv['ENV']['PATH']

    _vasm_builder = SCons.Builder.Builder(
        action = SCons.Action.Action('$VASM_COM' ,'$VASM_COMSTR'),
        suffix = '$VASM_OUTSUFFIX',
        src_suffix = '$VASM_SUFFIX')

    targetEnv.SetDefault(

        VASM_FLAGS = SCons.Util.CLVar(VASMFLAGS),

        VASM_OUTSUFFIX = '.o',
        VASM_SUFFIX = '.s',

        VASM_COM = 'vasmm68k_mot $VASM_FLAGS -I ${SOURCE.dir} -o $TARGET $SOURCE',
        VASM_COMSTR = ''
        )

    targetEnv.Append(BUILDERS = {'Vasm' : _vasm_builder})

    return targetEnv

def detectLibCMini(targetEnv):
    libcminiPath = os.environ.get('LIBCMINI')
    if libcminiPath:
        print "Using libcmini in: " + libcminiPath
        targetEnv.Append(LIBS=['iiomini', 'cmini', 'gcc'])
        targetEnv.Append(LIBPATH=[libcminiPath])
        targetEnv.Append(CCFLAGS='-nostdlib ')
        targetEnv.Append(LINKFLAGS='-nostdlib' + ' ' + libcminiPath + '/startup.o')
        targetEnv.Append(CPPPATH=libcminiPath + '/../include')
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

def setFastRamFlags(env, target):
    env.AddPostAction(target, Action('m68k-atari-mint-flags --mfastram --mfastload --mfastalloc $TARGET'))

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

debugBuildFlag=1

hostEnv = Environment(ENV = {'PATH' : os.environ['PATH']} )
targetEnv = setupToolchain(hostEnv.Clone(), debugBuildFlag)
networkDevice = 'svethlana'

# Optionally use libcmini
detectLibCMini(targetEnv)

targetEnv.Append(CPPDEFINES={'VERSION' : getVersion(hostEnv)})
targetEnv.Append(CPPDEFINES={'DEBUG' : debugBuildFlag})
targetEnv.Append(CPPDEFINES={'DUIP_CONF_BYTE_ORDER' : "BIG_ENDIAN"})

print "Building in: " + builddir

target = hostEnv.SConscript(
    "src/SConscript",
    duplicate = 0,
    exports=['hostEnv', 'targetEnv','networkDevice'],
    variant_dir = builddir,
    src_dir = "../" )

# Optionally compress the binary with UPX
compressProgramMaybe(targetEnv, target)
# Load into TT ram if possible
setFastRamFlags(targetEnv, target)

num_cpu = int(os.environ.get('NUMBER_OF_PROCESSORS', 2))
SetOption('num_jobs', num_cpu)
print "running with ", GetOption('num_jobs'), "jobs." 

Default(target)
