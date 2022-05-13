BUILDDIR        = '#build/release'
DISTDIR         = '#dist'
CXX             = 'cl'
CC              = 'cl'
# /O2=optimize for speed, global optimizations, intrinsic functions, favor fast code, frame pointer omission
# /EHsc=C++ exceptions, /fp:fast=Enable reasonable FP optimizations, /GS-=No buffer security checks, /GL=whole program optimizations
# To include debug information add '/Z7' to CXXFLAGS and '/DEBUG' to LINKFLAGS
# EDIT: MSVC2017 bugs Mitsuba when using /O2 because of /Ob2 - replaced it with the full set and /Ob2 for /Ob1
# EDIT: /Og is deprecated too
CXXFLAGS        = ['/nologo', #'/Og',
'/Oi', '/Ot', '/Oy', '/Ob1', #'/Ob2',
# '/Od', '/Z7'
'/Gs', '/GF', '/Gy', #'/O2',
'/fp:fast', '/DWIN32', '/DWIN64', '/W3', '/EHsc', '/GS-', '/GL', '/MD', '/DMTS_DEBUG', '/DSINGLE_PRECISION', '/DSPECTRUM_SAMPLES=3', '/DMTS_SSE', '/DMTS_HAS_COHERENT_RT', '/D_CONSOLE', '/DNDEBUG', '/openmp', '/D_USE_MATH_DEFINES', '/std:c++17']
SHCXXFLAGS      = CXXFLAGS
TARGET_ARCH     = 'x86_64'
MSVC_VERSION    = '14.3'
LINKFLAGS       = ['/nologo', '/SUBSYSTEM:CONSOLE', '/MACHINE:X64', '/FIXED:NO', '/OPT:REF', '/OPT:ICF', '/LTCG', '/NODEFAULTLIB:LIBCMT', '/MANIFEST']
BASEINCLUDE     = ['#include']
BASELIB         = ['msvcrt', 'ws2_32']#, 'Half', 'zlib']
# BASELIBDIR      = ['#dependencies/lib']
# OEXRINCLUDE     = ['#dependencies/include/openexr']
# OEXRLIB         = ['IlmImf', 'IlmThread', 'Iex', 'zlib']
# BOOSTLIB        = ['boost_system-vc141-mt-1_64', 'boost_filesystem-vc141-mt-1_64', 'boost_thread-vc141-mt-1_64']
# COLLADAINCLUDE  = ['#dependencies/include/collada-dom', '#dependencies/include/collada-dom/1.4']
# COLLADALIB      = ['libcollada14dom24']
# XERCESLIB       = ['xerces-c_3']
# PNGLIB          = ['libpng16']
# JPEGLIB         = ['jpeg']
GLLIB           = ['opengl32', 'glu32', 'gdi32', 'user32']
# GLFLAGS         = ['/D', 'GLEW_MX']
SHLIBPREFIX     = 'lib'
SHLIBSUFFIX     = '.dll'
LIBSUFFIX       = '.lib'
PROGSUFFIX      = '.exe'
# do not build python
# PYTHON27LIB     = ['boost_python27-vc141-mt-1_64', 'python27']
# PYTHON27INCLUDE = ['#dependencies/include/python27']
# PYTHON35LIB     = ['boost_python35-vc141-mt-1_64', 'python35']
# PYTHON35INCLUDE = ['#dependencies/include/python35']
# PYTHON36LIB     = ['boost_python36-vc141-mt-1_64', 'python36']
# PYTHON36INCLUDE = ['#dependencies/include/python36']
# use conan
# QTINCLUDE       = ['C:/soft/Qt/6.2.4/msvc2019_64/include']
# QTDIR           = 'C:/soft/Qt/6.2.4/msvc2019_64'
# FFTWLIB         = ['libfftw3-3']
