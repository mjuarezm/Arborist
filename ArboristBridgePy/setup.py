from distutils import log
from distutils.command.build_clib import build_clib as _build_clib
from distutils.core import setup
from distutils.extension import Extension
from Cython.Build import cythonize
from Cython.Distutils import build_ext as _build_ext
import numpy as np
from os import listdir, path



PKGNAME = 'pyborist'
DESCRIPTION = 'a Random Forest library'
with open('README.rst') as f:
    LONG_DESCRIPTION = f.read()
LICENSE = 'MIT'
VERSION = '0.0.1'



pyx_src_dir = 'pyborist'
cc_src_dir = path.join('..', 'ArboristCore')


all_pyx_files = [x for x in listdir(pyx_src_dir) if x.endswith('.pyx')]
all_cpp_core_files = [path.join(cc_src_dir, x) 
    for x in listdir(cc_src_dir) if x.endswith('.cc')] + \
    [path.join(pyx_src_dir, 'callback.cc')]


lib_aborist_core = ('libaboristcore', 
    {
        'sources': all_cpp_core_files,
        'include_dirs': [cc_src_dir, pyx_src_dir]
    }
)


extensions = [
    Extension('{}.{}'.format(PKGNAME, x[:-4]),
        [path.join(pyx_src_dir, x)],
        language = 'c++',
        include_dirs = [pyx_src_dir, cc_src_dir, np.get_include()],
        library_dirs = [pyx_src_dir, cc_src_dir]
    ) for x in all_pyx_files
]



win_compile_args = ['/openmp', '/Ox', '/fp:fast']
win_link_args = []
unix_compile_args = ['-std=c++11', '-fopenmp','-O3','-ffast-math']
unix_link_args = ['-fopenmp']


class build_clib(_build_clib):
    def build_libraries(self, libraries):
        """
        The default version does not accept extra compile args!!!
        So the hack is needed here. 
        Most code are exactly the same but the compiler.compile() gets injected.
        """
        compiler_type = self.compiler.compiler_type
        if compiler_type == 'msvc':
            compile_args = win_compile_args
        else:
            compile_args = unix_compile_args

        for (lib_name, build_info) in libraries:
            sources = list(build_info.get('sources'))
            log.info("building '%s' library", lib_name)
            macros = build_info.get('macros')
            include_dirs = build_info.get('include_dirs')
            objects = self.compiler.compile(sources,
                                            output_dir=self.build_temp,
                                            macros=macros,
                                            include_dirs=include_dirs,
                                            extra_postargs=compile_args, # important
                                            debug=self.debug)
            self.compiler.create_static_lib(objects, lib_name,
                                            output_dir=self.build_clib,
                                            debug=self.debug)


class build_ext(_build_ext):
    def build_extensions(self):
        """
        http://stackoverflow.com/questions/724664/python-distutils-how-to-get-a-compiler-that-is-going-to-be-used

        We want to inject the compile / link args based on different compilers.
        """
        compiler_type = self.compiler.compiler_type
        for ext in self.extensions:
            if compiler_type == 'msvc':
                ext.extra_compile_args = win_compile_args
                ext.extra_link_args = win_link_args
            else:
                ext.extra_compile_args = unix_compile_args
                ext.extra_link_args = unix_link_args
        _build_ext.build_extensions(self)


setup(
    name = PKGNAME,
    version = VERSION,
    description = DESCRIPTION,
    long_description = LONG_DESCRIPTION,
    license = LICENSE,
    packages = ['pyborist'],
    libraries = [lib_aborist_core],
    cmdclass = {'build_clib': build_clib, 'build_ext': build_ext},
    ext_modules = cythonize(extensions)
)
