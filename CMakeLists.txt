cmake_minimum_required(VERSION 3.18)

project(benchmark LANGUAGES C)

# Sources.
add_executable(benchmark
	"benchmark.c"
)

set_property(DIRECTORY . PROPERTY VS_STARTUP_PROJECT benchmark)

if(NINTENDO_3DS)
	ctr_create_3dsx(benchmark)
else()
	# Only require C90.
	set_target_properties(benchmark PROPERTIES
		C_STANDARD 90
		C_STANDARD_REQUIRED NO
		C_EXTENSIONS OFF
	)
endif()

# Link clownmdemu core.
add_subdirectory("clownmdemu" EXCLUDE_FROM_ALL)
target_link_libraries(benchmark clownmdemu-core)

# Link libm (for some reason this isn't automatic).
find_library(M m)
if(DEFINED M)
	target_link_libraries(benchmark m)
endif()
