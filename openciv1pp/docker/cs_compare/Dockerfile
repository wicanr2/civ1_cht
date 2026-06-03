FROM mcr.microsoft.com/dotnet/sdk:8.0
ENV DEBIAN_FRONTEND=noninteractive TZ=Asia/Taipei
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake pkg-config git ca-certificates \
    libsdl2-dev libfreetype6-dev \
    xvfb x11-utils xdotool ffmpeg imagemagick \
    fonts-noto-cjk fonts-arphic-uming \
    libice6 libsm6 libfontconfig1 libx11-xcb1 libxcb-glx0 libxcb-render0 \
    libxcb-shape0 libxcb-shm0 libxrender1 libxcursor1 libxext6 libxrandr2 \
    libxi6 libxss1 libgl1 libglib2.0-0 libxkbcommon0 python3 python3-pil \
 && rm -rf /var/lib/apt/lists/*

# Pre-warm the NuGet cache so `--network none` runs can still build OpenCiv1.
# We run a restore for the same package set as src/OpenCiv1.csproj. The image
# build phase is the only point that has network access; runtime is offline.
# Use the dependency-resolution mode that lets NuGet pull SkiaSharp 2.88.9 too
# (Avalonia.Skia 11.3.14 transitively requires it).
WORKDIR /prewarm
COPY prewarm.csproj /prewarm/prewarm.csproj
RUN dotnet restore /prewarm/prewarm.csproj && rm -rf /prewarm
ENV NUGET_PACKAGES=/root/.nuget/packages

WORKDIR /work
CMD ["/bin/bash"]
