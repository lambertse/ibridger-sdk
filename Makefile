CPP_BUILD_DIR := sdk/cpp/build
CPP_FLAGS     ?= -DCMAKE_BUILD_TYPE=Release

.PHONY: all build test clean \
        build-cpp build-js \
        test-cpp test-js test-integration \
        generate-proto

# ─── Top-level targets ────────────────────────────────────────────────────────

all: build

build: build-cpp build-js

test: test-cpp test-js test-integration

# ─── C++ SDK ──────────────────────────────────────────────────────────────────

build-cpp: $(CPP_BUILD_DIR)/Makefile
	cmake --build $(CPP_BUILD_DIR)

$(CPP_BUILD_DIR)/Makefile:
	cmake -B $(CPP_BUILD_DIR) sdk/cpp \
	  $(CPP_FLAGS) \
	  -DIBRIDGER_BUILD_TESTS=ON \
	  -DIBRIDGER_BUILD_EXAMPLES=ON

test-cpp: build-cpp
	cd $(CPP_BUILD_DIR) && ctest --output-on-failure

# ─── JS SDK ───────────────────────────────────────────────────────────────────

build-js:
	cd sdk/js && npm install && npm run build

test-js: build-js
	cd sdk/js && npm test

# ─── Integration tests ────────────────────────────────────────────────────────

test-integration: build-cpp build-js
	cd tests/integration && npm install && npm test

# ─── Proto generation ─────────────────────────────────────────────────────────

generate-proto:
	cd sdk/js && npm run generate-proto

# ─── Clean ────────────────────────────────────────────────────────────────────

clean:
	rm -rf $(CPP_BUILD_DIR) sdk/js/dist
