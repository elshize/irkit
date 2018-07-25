# IRKit

Information Retrieval tools intended for academic research.

Web: [https://elshize.github.io/irkit/](https://elshize.github.io/irkit/)

# Primary Goals

1. Composability and flexibility.
2. Modern and clean code.
3. Efficiency.

These goals should be achieved iteratively.

# Documentation

[https://elshize.github.io/irkit/docs/html/](https://elshize.github.io/irkit/docs/html/)

# Installation

We recommend using `conan` package manager.
You can install it using `pip`:

```sh
export PATH=$HOME/.local/bin:$PATH
pip install --user conan
```

With `conan` installed, use it to install dependencies, and compile the project:

```sh
mkdir build && cd build
conan install .. --build=missing
```

You can also choose to use system installation of Boost:

```sh
mkdir build && cd build
conan install .. --build=missing -o irkit:use_system_boost=True
```

Once this is done, you can use `cmake` to build and install.

```sh
cmake ..
cmake --build .  # or: make
sudo cmake --build . --target install  # or: sudo make install
```

## Conan Package

If you want to use `irkit` as a dependency in your project,
you can use the `conan` package. First, must add the remote repository:

```sh
conan remote add irkit https://api.bintray.com/conan/elshize/irkit
```

Then install locally:

```sh
conan install irkit/0.1@elshize/develop
```

or use in your `conanfile.py`.
