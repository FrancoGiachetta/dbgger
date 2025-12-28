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
	
.PHONY: fmt
fmt:
	find . \
		\( -iname 'vcpkg' -o -iname 'builds' \) -prune \
		-o -type f \( -iname '*.h' -o -iname '*.cpp' -o -iname '*.hpp' \) -print \
		| clang-format --style=Microsoft -i --files=/dev/stdin
		

.PHONY: clean
clean:
	rm -rf ${BUILDS_ROOT} .cache
	rm -f compile_commands.json
