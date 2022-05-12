# Modified version of mitsuba
[![CI](https://github.com/xehoth/mitsuba/actions/workflows/docker-image.yml/badge.svg)](https://github.com/xehoth/mitsuba/actions/workflows/docker-image.yml)

Modified version of mitsuba renderer target for learning rendering.

## Changes

- Use python3 and scons3 for compiling.
- Generate compile commands with [scons-compiledb](https://pypi.org/project/scons-compiledb/). Thus we can use clangd for intellisense.
- Add support for package manager [conan](https://conan.io/)
- Fix compilation with C++ 17
- Fix overflow in render setting UI (only happens in release mode)
- OpenEXR has been changed to 2.5.5 with conan.

## New Plugins

- [openvdbvolume](#openvdbvolume): openvdb volume data source
- [nanovdbvolume](#nanovdbvolume): nanovdb volume data source
- [ratiotracking](#ratiotracking): transmittance estimation for heterogeneous medium
- [nextflight](#nextflight): next flight estimator for transmittance estimation
- [pseriesratio](#pseriesratio): P-series ratio tracking estimator for transmittance estimation
- [pseriescumulative](#pseriescumulative): P-series cumulative estimator for transmittance estimation
- [pseriescmf](#pseriescmf): P-series CMF estimator for transmittance estimation
- [multipass_volpath](#multipass_volpath): modified `volpath` integrator that supports multiple passes

May implement some recent papers.

### openvdbvolume

Implemented in file `volume/openvdbvolume.cpp`.  
- `filename`: the file path of the openvdb file
- `gridname`: explicitly use the given grid in the vdb, if not given, use the first one
- `customStepSize`: default set to half of the voxel size

Example:
```xml
<volume name="density" type="openvdbvolume">
  <string name="filename" value="bunny_cloud.vdb"/>
  <transform name="toWorld">
    <scale value="0.05"/>
    <translate y="-1"/>
  </transform>
</volume>
```

### nanovdbvolume

Implemented in file `volume/nanovdbvolume.cpp`.  
- `filename`: the file path of the openvdb file
- `gridname`: explicitly use the given grid in the vdb, if not given, use the first one
- `customStepSize`: default set to half of the voxel size

Example:
```xml
<volume name="density" type="nanovdbvolume">
  <string name="filename" value="bunny_cloud.vdb"/>
  <transform name="toWorld">
    <scale value="0.05"/>
    <translate y="-1"/>
  </transform>
</volume>
```

### ratiotracking

Implemented in file `medium/heterogeneous.cpp`.  
Novák J, Selle A, Jarosz W. Residual ratio tracking for estimating attenuation in participating media[J]. ACM Trans. Graph., 2014, 33(6): 179:1-179:11.  
See details: [https://cs.dartmouth.edu/wjarosz/publications/novak14residual.html](https://cs.dartmouth.edu/wjarosz/publications/novak14residual.html)

Example:
```xml
<medium type="heterogeneous" id="smoke">
  <string name="method" value="ratiotracking"/>
</medium>
```

### nextflight

Implemented in file `medium/heterogeneous.cpp`.  
Novák J, Georgiev I, Hanika J, et al. Monte Carlo methods for volumetric light transport simulation[C]//Computer Graphics Forum. 2018, 37(2).  
See details: [https://cs.dartmouth.edu/wjarosz/publications/novak18monte.html](https://cs.dartmouth.edu/wjarosz/publications/novak18monte.html)

Example:
```xml
<medium type="heterogeneous" id="smoke">
  <string name="method" value="nextflight"/>
</medium>
```

### pseriesratio

Implemented in file `medium/heterogeneous.cpp`.  
Georgiev I, Misso Z, Hachisuka T, et al. Integral formulations of volumetric transmittance[J]. ACM Transactions on Graphics (TOG), 2019.  
See details: [https://cs.dartmouth.edu/wjarosz/publications/georgiev19integral.html](https://cs.dartmouth.edu/wjarosz/publications/georgiev19integral.html)

Example:
```xml
<medium type="heterogeneous" id="smoke">
  <string name="method" value="pseriesratio"/>
</medium>
```

### pseriescumulative

Implemented in file `medium/heterogeneous.cpp`.  
Georgiev I, Misso Z, Hachisuka T, et al. Integral formulations of volumetric transmittance[J]. ACM Transactions on Graphics (TOG), 2019.  
See details: [https://cs.dartmouth.edu/wjarosz/publications/georgiev19integral.html](https://cs.dartmouth.edu/wjarosz/publications/georgiev19integral.html)  
One note in implementation: use `double` to accumulate `W`, it may become `inf` when grid domain is large when using float.

Example:
```xml
<medium type="heterogeneous" id="smoke">
  <string name="method" value="pseriescumulative"/>
</medium>
```

### pseriescmf

Implemented in file `medium/heterogeneous.cpp`.  
Georgiev I, Misso Z, Hachisuka T, et al. Integral formulations of volumetric transmittance[J]. ACM Transactions on Graphics (TOG), 2019.  
See details: [https://cs.dartmouth.edu/wjarosz/publications/georgiev19integral.html](https://cs.dartmouth.edu/wjarosz/publications/georgiev19integral.html)  

Example:
```xml
<medium type="heterogeneous" id="smoke">
  <string name="method" value="pseriescmf"/>
</medium>
```

### multipass_volpath

Implemented in file `integrators/path/multipass_volpath.cpp`.  
Modify the `volpath` integrator to support multiple passes. More interactive and a try because some other integrators may some operations between different pass.  

- `sppPerPass`: the spp for each pass, the pass count will be calculated automatically according to total sample count. Do **not** use with `passCount`.
- `passCount`: the total pass count, spp per pass will be calculated automatically according to total sample count. Do **not** use with `sppPerPass`.

Example:
```xml
<integrator type="multipass_volpath">
  <integer name="maxDepth" value="8"/>
  <integer name="sppPerPass" value="4"/>
</integrator>
```

## Dockerfile

Quickly build with docker. And the compile process can refer to this.

# Mitsuba — Physically Based Renderer

http://mitsuba-renderer.org/

## About

Mitsuba is a research-oriented rendering system in the style of PBRT, from which it derives much inspiration. It is written in portable C++, implements unbiased as well as biased techniques, and contains heavy optimizations targeted towards current CPU architectures. Mitsuba is extremely modular: it consists of a small set of core libraries and over 100 different plugins that implement functionality ranging from materials and light sources to complete rendering algorithms.

In comparison to other open source renderers, Mitsuba places a strong emphasis on experimental rendering techniques, such as path-based formulations of Metropolis Light Transport and volumetric modeling approaches. Thus, it may be of genuine interest to those who would like to experiment with such techniques that haven't yet found their way into mainstream renderers, and it also provides a solid foundation for research in this domain.

The renderer currently runs on Linux, MacOS X and Microsoft Windows and makes use of SSE2 optimizations on x86 and x86_64 platforms. So far, its main use has been as a testbed for algorithm development in computer graphics, but there are many other interesting applications.

Mitsuba comes with a command-line interface as well as a graphical frontend to interactively explore scenes. While navigating, a rough preview is shown that becomes increasingly accurate as soon as all movements are stopped. Once a viewpoint has been chosen, a wide range of rendering techniques can be used to generate images, and their parameters can be tuned from within the program.

## Documentation

For compilation, usage, and a full plugin reference, please see the [official documentation](http://mitsuba-renderer.org/docs.html).

## Releases and scenes

Pre-built binaries, as well as example scenes, are available on the [Mitsuba website](http://mitsuba-renderer.org/download.html).
