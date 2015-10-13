include config.make

# make sure the the OF_ROOT location is defined
ifndef OF_ROOT
    OF_ROOT=/home/eddiew/Downloads/of_v0.8.4_linux64_release/
endif

# call the project makefile!
include $(OF_ROOT)/libs/openFrameworksCompiled/project/makefileCommon/compile.project.mk
