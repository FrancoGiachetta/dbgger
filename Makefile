BUILDS_ROOT=builds
BUILD_DIR=${BUILDS_ROOT}/build
VCPKG_CMAKE=vcpkg/scripts/buildsystems/vcpkg.cmake

.PHONY: build
build: clean
	cmake -S . -B ${BUILD_DIR} --fresh -DCMAKE_TOOLCHAIN_FILE=${VCPKG_CMAKE}
	cmake --build ${BUILD_DIR} 

.PHONY: test
test: clean
	cmake -S . -B ${BUILD_DIR} --fresh -DCMAKE_TOOLCHAIN_FILE=${VCPKG_CMAKE} -DBUILD_TESTING=ON
	cmake --build ${BUILD_DIR} 
	
.PHONY: clean
clean:
	rm -rf ${BUILDS_ROOT}
	rm -f compile_commands.json
