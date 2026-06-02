FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive TZ=Asia/Taipei
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake pkg-config git ca-certificates \
    libsdl2-dev libfreetype6-dev \
    xvfb x11-utils xdotool ffmpeg \
    dosbox unzip imagemagick fonts-noto-cjk \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /work
CMD ["/bin/bash"]
