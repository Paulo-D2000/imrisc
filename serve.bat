make -f Makefile.emscripten all
copy /Y program.bin web\
python serve_wasm.py