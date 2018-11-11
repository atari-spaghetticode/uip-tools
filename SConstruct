import os

builddir = os.path.abspath(GetLaunchDir())
SConsignFile(os.path.join(builddir, '.sconsign.dblite'))

def setupToolchain(targetEnv):   
	CROSS_PREFIX = 'm68k-atari-mint-'

	targetEnv["CC"] = CROSS_PREFIX + 'gcc'
	targetEnv["CXX"] = CROSS_PREFIX + 'g++'

	targetEnv['AR'] = CROSS_PREFIX + 'ar'
	targetEnv['RANLIB'] = CROSS_PREFIX + 'ranlib'
	targetEnv['OBJCOPY'] = CROSS_PREFIX + 'objcopy'
	targetEnv['STRIP'] = CROSS_PREFIX + 'strip -s'
	targetEnv['STACK'] = CROSS_PREFIX + 'stack'

	targetEnv['VASM_FLAGS'] = '-Faout -quiet'

	targetEnv['CXXFLAGS'] = '-fno-rtti -fno-exceptions -fomit-frame-pointer -ffast-math -I${TARGET.dir}' #-Werror -Wall
	targetEnv['CCFLAGS'] = '-std=gnu99 -fomit-frame-pointer -m68030 -m68881 -ffast-math -I${TARGET.dir}' #-Werror -Wall
	targetEnv['LINKFLAGS'] = '-fno-exceptions -fno-rtti -fomit-frame-pointer'

	# Add sensible toolchain detection?
	targetEnv['ENV']['PATH'] = "/opt/cross-mint/bin:" + targetEnv['ENV']['PATH']

	return targetEnv

hostEnv = Environment(ENV = {'PATH' : os.environ['PATH']} )
targetEnv = setupToolchain(hostEnv.Clone())

targetEnv.Append(CPPDEFINES={'VERSION' : "99"})
targetEnv.Append(CPPDEFINES={'DUIP_CONF_BYTE_ORDER' : "BIG_ENDIAN"})

print GetLaunchDir()
targets = hostEnv.SConscript(
	"src/SConscript",
	duplicate = 0,
	exports=['hostEnv', 'targetEnv'],
	variant_dir = builddir,
	src_dir = "../" )

num_cpu = int(os.environ.get('NUMBER_OF_PROCESSORS', 2))
SetOption('num_jobs', num_cpu)
print "running with ", GetOption('num_jobs'), "jobs." 

Default (targets)
