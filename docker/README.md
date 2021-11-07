# Build with Docker

If you prefer to build i3status inside of a Docker container, from the main directory of this repo, do the following:

1. build the image using the Dockerfile in the `docker` directory:
   ```
   sudo docker build -t i3status-meson-builder ./docker
   ```
2. start the build:
   ```
   sudo docker run -it -v `pwd`:/src i3status-meson-builder
   ```
3. find your `i3status` binary in the `./build/` directory, move it to `/usr/bin`
   ```
   sudo mv ./build/i3status /usr/bin/i3status
   ```
